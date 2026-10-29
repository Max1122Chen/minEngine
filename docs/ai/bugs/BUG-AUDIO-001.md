# BUG-AUDIO-001 — 音频剪辑被释放而设备线程仍在读其 PCM

## Meta
- **ID:** BUG-AUDIO-001
- **Status:** Fixed
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-10-29
- **Last updated:** 2026-10-29
- **Affects:** `AudioVoice`、`AudioSystem::Play/Play2D/Play3D`、`MiniaudioBackend`、`IAudioBackend`、`MockAudioBackend`
- **Related Feature/Slice:** 音频运行时 (`Runtime/Function/Audio`)

## TL;DR
`AudioVoice` 只用 `std::weak_ptr<AudioClip>` 观察剪辑，而 `MiniaudioBackend::CreateVoice` 把
`clip.GetPcmData().data()` 交给 `ma_audio_buffer_init`，miniaudio 不拷贝采样。于是任何不经
`AudioComponent::Stop` 的释放（`Play2D/Play3D` 返回后调用方丢掉 `shared_ptr`、资产被卸载）都会让设备线程
读已释放的 `std::vector<float>`——纯 UAF，无日志无断言。已修复：voice 与后端 voice 各自持强引用，
裸指针只能经 `AudioClipLease` 交出，并带断言。

---

## 症状
- `AudioSystem::Play2D(clip)` 之后调用方 `clip.reset()`：剪辑立刻析构，播放中的 voice 仍在读它的采样。
- 崩溃位置随机（miniaudio 设备线程），可能是杂音、静音，或访问违例；进程内没有任何错误日志。
- `AssetManager` 的剪辑缓存也是 weak，资产卸载路径同样触发。

## 期望
- 一个 voice 从开始播放到被停止/自然结束之间，其采样所属的剪辑必须存活（`use_count() >= 1`）。
- 后端 voice 销毁与剪辑释放有确定顺序：先 `ma_sound_uninit` → `ma_audio_buffer_uninit`，再放剪辑。
- 如果将来有人再次把裸 `pcm.data()` 交给设备线程而没有 owner，应当断言/记录，而不是静默 UAF。

## 复现
1. `minEngineTests.exe test audio-smoke`（修复前）。
2. 用例 "clip outlives the caller that started playback" 丢弃外部 `shared_ptr` 后：
   `AudioSmokeTest: clip died while the voice was still playing.`
3. 其余三条同族用例分别在"Stop / 自然结束 / 最后一个 voice"处以
   `playing voice holds no reference to the clip`、`clip was released while the second voice still plays` 失败。

## 环境
- Windows，契约构建 `minEngine/build`（Debug），产物 `minEngine/bin`。

## 根因
两处所有权缺口叠加：
1. `AudioVoice::m_Clip` 是 `weak_ptr`，`EnsureBackendVoice` 里临时 `lock()` 一下就把裸指针交给后端，
   返回后强引用归零。
2. `IAudioBackend::CreateVoice(const AudioClip&)` 只拿到引用，后端没有任何手段延长剪辑寿命，却把
   `pcm.data()` 存进了 `ma_audio_buffer`（miniaudio 明确不拷贝采样）。

## 修复
- 新增 `AudioClipLease`（`Runtime/Function/Audio/AudioClipLease.h`，非导出）：持有 `shared_ptr<const AudioClip>`
  并缓存采样指针；`GetPcmData()` 每次校验"指针仍属于该剪辑"，违规时 `ME_ASSERT`（Debug 触发，
  Release 记录）并返回 `nullptr`，因此不导出计数也不会把失效指针交出去。
- `IAudioBackend::CreateVoice` 改为接收 `std::shared_ptr<const AudioClip>`（所有权而非引用）；
  `MiniaudioBackend::VoiceSlot` 存 `AudioClipLease`，在 `DestroyVoice`/`Shutdown` 中于
  `ma_audio_buffer_uninit` 之后 `Reset()`。
- `AudioVoice::m_Clip` 改为强 `shared_ptr`，在 `Stop`/`NotifyPlaybackFinished`/`Reset` 释放，
  且释放排在 `DestroyVoice` 之后。
- `AudioVoice::Play` 返回 `bool`；`AudioSystem::Play` 在启动失败时立即 `StopAndFreeVoice`，
  避免"从未播放的 voice 永久持有剪辑"。

## 回归验证
- `cmake --build minEngine/build --target minEngineTests` / `--target Editor` 通过。
- `minEngineTests.exe test audio-smoke`（在 `minEngine/bin` 下）：6 cases / 19 assertions PASS，
  其中 4 条为本次新增的所有权用例，修复前全部红灯。
- `minEngineTests.exe test smoke`：exit 0。
- 导出快照未变化（新增类型全部非导出，`AudioVoice`/`AudioClipLease` 不在白名单中）。

## 关联
- 渲染侧同原则：`PrimitiveSceneProxy` 持 `RHIBufferRef`，即"使用方持有自己要用到的资源"。
- 同族审计结论见 `docs/ai/Audio/AUDIO_PCM_OWNERSHIP.md`。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-29 | 记录并修复：voice/后端强引用 + `AudioClipLease` 断言 |
