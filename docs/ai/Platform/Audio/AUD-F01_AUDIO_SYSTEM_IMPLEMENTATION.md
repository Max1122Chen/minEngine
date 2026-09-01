# Audio System — Implementation Plan

## Meta
- **ID:** `AUD-F01`
- **Type:** Implementation
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-01
- **Related:** [Design](./AUD-F01_AUDIO_SYSTEM_DESIGN.md), [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md)

## TL;DR

按 **Backend → Asset → Voice → System → 2D → Component → Listener → 3D → Bus → 生命周期 → Demo → Tests** 顺序竖切；每切片可编译验证，不一次性提交全模块。

---

## 切片总览

| Slice | 内容 | 验收 |
|-------|------|------|
| **S01** | Vendor miniaudio；`IAudioBackend`；`MiniaudioBackend::Initialize/Shutdown/Update` | 链接通过；可初始化设备（无播放） |
| **S02** | `AudioClip` + `AudioClipLoader` + `AssetTypeRegistry`（`.wav`，可选 `.ogg`） | `LoadAsset<AudioClip>` 成功；headless 读入测试文件 |
| **S03** | `AudioTypes` / `AudioVoice` + `AudioSystem` 骨架；`Play2D` / `Stop`；`MockAudioBackend`（headless） | smoke 可断言 Play/Stop；真机可选 MiniaudioBackend |
| **S04** | `AudioMixer` / `AudioBus`（Master/Music/SFX/UI）；`SetBusVolume` / `Mute` | Bus mute 后无声 |
| **S05** | `AudioComponent`；与 GameObject / Scene 关联 | 组件 `Play()` 发声 |
| **S06** | `AudioListenerComponent`；System 每帧同步 Listener | 3D API 调用链打通（可先测位置传递） |
| **S07** | 3D：Min/Max distance、衰减、spatialization | 远离 Listener 音量明显降低 |
| **S08** | Pause / Resume / Loop / Pitch | smoke 子用例 |
| **S09** | 生命周期：Component 销毁、Scene 卸载、Clip weak_ptr 策略 | 重复 smoke 无泄漏/崩溃 |
| **S10** | Playground 或测试场景手动 Demo | 人耳验收 |
| **S11** | `test audio-smoke` suite；`verify.ps1` 登记（若需） | CI 本地命令文档化 |

**MVP Done 门槛：** S01–S09 + S11 ✅（2026-09-01）。S10 Editor 人耳验收 ✅。

**后续切片（非 MVP）：** 衰减默认值/模型 Inspector、自定义曲线 → 登记 `AUD-F02` 或按需小改。

---

## S01 — Backend 竖切

**文件：**
- `Third-Party/miniaudio/miniaudio.h` + `miniaudio.c`
- `Runtime/Function/Audio/AudioTypes.h`、`AudioLimits.h`
- `Runtime/Function/Audio/Backend/AudioBackendTypes.h`
- `Runtime/Function/Audio/Backend/IAudioBackend.h`
- `Runtime/Function/Audio/Backend/MiniaudioBackend.*`（pimpl；仅 `.cpp` include miniaudio）

**步骤：**
1. CMake 加入 miniaudio 实现单元（`MINIAUDIO_IMPLEMENTATION` 一处）。
2. `MiniaudioBackend::Initialize` 创建 `ma_engine`（或等价设备）。
3. `Shutdown` 释放设备。
4. `Engine` **暂不**挂钩；用临时 test 或 `AudioBackendTestScope` 验证 Init/Shutdown。

**验证：**
```powershell
cmake --build minEngine/build --target minEngineTests
minEngine\bin\minEngineTests.exe test audio-smoke   # S01 可能仅占位通过或独立 backend 用例
```

---

## S02 — AudioClip 资产

**参照：** `LuaScript` + `LuaScriptLoader`、`AssetTypeRegistry::RegisterBuiltinTypes`。

**步骤：**
1. `AudioClip : Asset`，内存缓冲 + 格式元数据。
2. `AudioClipLoader::Load` — 磁盘解码（miniaudio `ma_decoder` 读入 PCM，或 Backend 侧延迟解码 **二选一**；Design 倾向 **Loader 解码入 Clip**，Backend 从 PCM 创建 sound）。
3. 注册 `AssetTypeId` `"AudioClip"`，扩展名 `.wav`（+ `.ogg`）。
4. 测试资产：`minEngineTests/Fixtures/Audio/` 下放短 wav。

---

## S03 — Voice + System 2D

**步骤：**
1. `AudioSystem` 单例；`Engine::StartSystems` / `ShutdownSystems` / `LogicalTick`。
2. `Play2D(shared_ptr<AudioClip>)` → 创建 Voice → Backend Play。
3. `StopAll` on Shutdown。

---

## S04 — Bus

**步骤：**
1. `AudioMixer` 静态默认 Bus 表。
2. Voice 播放时带 `AudioBusId`；`SetVolume` 时乘 Bus × Master。

---

## S05–S07 — 场景 3D

**步骤：**
1. `AudioComponent` 反射 + `Play`/`Stop`。
2. `AudioListenerComponent`；System 收集 active listener。
3. 每帧 `SetListener` + 对每个 spatialized voice `SetVoicePosition` + spatial settings。

---

## S09 — 生命周期

**步骤：**
1. `AudioComponent` 析构 → `AudioSystem::UnregisterComponent`。
2. `SceneManager::UnloadActiveScene` 在 `PhysicsSystem::DestroyWorld` **之前**调用 `AudioSystem::OnSceneUnloaded`（见 Design §3.14）。
3. Clip：`weak_ptr` 检查；活跃 Voice 阻止 silent UAF。

---

## S11 — Tests

**参照：** `PhysicsSmokeTestScope`、`DelegateTest`。

**建议用例：**
- Load clip from fixture path
- Play2D + sleep + Stop
- Bus mute
- 3D attenuation（mock backend 或阈值）
- Destroy GameObject with AudioComponent

**注册：** `Tests/TestMain.cpp` 增加 `audio-smoke` suite。

---

## Engine 挂钩清单（S03 起）

```cpp
// Engine.h — 新增
std::shared_ptr<AudioSystem> m_AudioSystem;

// StartSystems — AssetManager 之后、LogicalTick 之前 Initialize
// LogicalTick — SceneManager::Tick 之后、Physics 之前
m_AudioSystem->Tick(deltaTime);

// ShutdownSystems — SceneManager 之后、AssetManager 之前 Shutdown Audio
```

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | MVP Done：S01–S11 + Editor 验收；post-MVP 集成修复见 PROGRESS_LOG |
| 2026-08-31 | 初稿 Implementation Plan（S01–S11） |
| 2026-08-31 | 对齐 Design v2（文件清单、MockBackend、SceneManager 钩子） |
