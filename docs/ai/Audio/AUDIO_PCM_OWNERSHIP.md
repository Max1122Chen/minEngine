# 音频侧"把内部指针交给设备线程"的所有权审计

范围：`Runtime/Function/Audio`（`AudioSystem`、`AudioVoice`、`AudioMixer`、`Backend/MiniaudioBackend`、`Components/*`）。

miniaudio 的设备线程是唯一会越过帧序读取运行时内存的读者，因此审计按"谁把指针交出去、谁负责在读完前保活"逐项核对。

## 结论表

| # | 交出点 | 交给谁 / 谁读 | 原状态 | 现在 | 结论 |
|---|--------|---------------|--------|------|------|
| 1 | `ma_audio_buffer_init(..., pcm.data(), ...)`（`MiniaudioBackend::CreateVoice`） | miniaudio 设备线程，持续整个 voice 生命周期 | 裸指针，owner 由调用方 `shared_ptr` 维持 → UAF | `AudioClipLease` 持有剪辑强引用，`GetPcmData()` 校验归属性 | 已修（BUG-AUDIO-001） |
| 2 | `ma_sound_config.pDataSource = &slot.Buffer` | miniaudio 设备线程，voice 生命周期 | 指向 `Impl::Voices` 元素成员 | 不变；voice 池在构造时一次 `resize(kMaxAudioVoices)`，之后不再增长/搬迁，且 `ma_sound_uninit` 先于 `ma_audio_buffer_uninit` | 安全（附约束注释：池大小固定，不得在运行期扩容） |
| 3 | `ma_engine_listener_set_position/direction/world_up`、`ma_sound_set_*` | miniaudio 内部拷贝数值 | 传值 | 不变 | 安全 |
| 4 | `AudioVoice::m_OwnerComponent`（`AudioComponent*`） | 无外部读者 | 裸指针，组件析构后可能悬空 | 不变 | 目前**无读者**（`SyncEmittersToBackend` 走 `emitter->GetActiveVoiceHandle()`），悬空指针不会被解引用；若将来要读，必须先换成句柄或弱引用 |
| 5 | `AudioVoice::m_OwnerScene`（`Scene*`） | `AudioSystem::OnSceneUnloaded`/`StopVoicesNotOwnedByScene` 指针比较 | 裸指针 | 不变 | 仅做相等比较；场景销毁路径会先 `StopAndFreeVoice` 清掉属于该场景的 voice，比较不会解引用 |
| 6 | `AudioSystem::m_ActiveListener`/`m_SuspendedEditorListener`（`AudioListenerComponent*`） | `AudioSystem::SyncListenerToBackend` 每帧解引用 | 裸指针 | 不变 | 安全：`AudioListenerComponent` 析构与 `RemoveActivationFromSystems` 都会 `UnregisterListener`，且 `UnregisterListener` 是唯一清除点 |
| 7 | `AudioSystem::m_Emitters`（`AudioComponent*` 数组） | 每帧 `SyncEmittersToBackend`/`ProcessPlayOnAwake` | 裸指针 | 不变 | 安全：组件析构与停用都会 `UnregisterEmitter` |
| 8 | `AudioMixer` 总线状态 | 主线程读取，值语义 | 无指针 | 不变 | 安全 |
| 9 | `AssetManager` 的剪辑缓存（weak） | 资产层 | 只观察 | 不变 | 安全：剪辑存活不再依赖缓存，播放路径已自持强引用 |
| 10 | `MockAudioBackend`（仅测试） | 模拟设备读 | 记录裸采样指针 | 改存 `AudioClipLease`，与真实后端同一条所有权路径 | 与生产一致 |

## 顺带发现（未改，留作后续）

- `AudioSystem::InitializeWithBackend` 失败时 `m_VoiceSlots` 未清空：只在"后端初始化失败且此前分配过 slot"时才可能残留，
  当前调用序列不会触发；改动会影响初始化语义，未纳入本次范围。
- `AudioClipLease` 缓存采样首地址：`AudioClip` 目前只在加载期写入 PCM，没有原地改写 API。若将来出现
  "播放中改写 PCM 并导致 `std::vector` 重新分配"的入口，租约的归属性断言会先报出来（Debug 触发，Release 记录并拒绝交出指针）。

## 最后一个引用何时归零

1. 播放中：外部持有者 + `AudioVoice::m_Clip` + `MiniaudioBackend::VoiceSlot::ClipLease`（同一剪辑被 N 个 voice 播放则各自一份）。
2. `Stop`/`StopAllVoices`/`OnSceneUnloaded`/PIE 切换：先 `StopVoice` → `DestroyVoice`（`ma_sound_uninit` →
   `ma_audio_buffer_uninit` → `ClipLease.Reset()`），再 `AudioVoice::m_Clip.reset()`。
3. 自然播放结束：`AudioSystem::Tick` 的 `UpdateVoiceStates` 发现 `IsVoicePlaying` 为假 → `StopAndFreeVoice`，走同一条释放路径。
4. 因此剪辑在"最后一个 voice 停止或结束、且所有外部 `shared_ptr` 都已释放"时归零；没有任何路径会让 voice 或后端无限持有它。
