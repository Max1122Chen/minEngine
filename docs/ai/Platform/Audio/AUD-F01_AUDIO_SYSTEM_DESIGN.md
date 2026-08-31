# Audio System — Design Spec

## Meta
- **ID:** `AUD-F01`
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-08-31
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../../ACTIVE_WORK.md), [ASSET_PIPELINE_DESIGN.md](../ContentBrowser/ASSET_PIPELINE_DESIGN.md), [ENGINE_STARTUP_DESIGN.md](../Startup/ENGINE_STARTUP_DESIGN.md), [Implementation](./AUD-F01_AUDIO_SYSTEM_IMPLEMENTATION.md)
- **Branch / worktree:** `feat/audio` · `D:/Dev/GitRepo/minEngine-audio`

## TL;DR

为 minEngine 增加 **中等规模 Runtime Audio System**：架构清晰、职责分层，能支撑完整 3D 游戏 Demo（2D/3D 播放、距离衰减、基础空间化、Bus 混音），**不追求** UE/Wwise 级功能。核心不变量：**AudioClip（资产）与 AudioVoice（播放实例）严格分离**；场景语义在 **AudioComponent / AudioListenerComponent**；协调在 **AudioSystem**；第三方 API 隔离在 **`IAudioBackend`**（首期 **MiniaudioBackend**）。

## Scope
- **In:**
  - `AudioClip` 资产类型 + `AssetManager` / `AssetTypeRegistry` 集成
  - `AudioVoice` 播放实例（Play / Stop / Pause / Resume / Volume / Pitch / Loop）
  - `AudioComponent`（场景发射器）+ `AudioListenerComponent`（听者，通常挂 Camera）
  - 2D 与基础 3D（位置、Listener、Min/Max Distance、距离衰减、立体声 pan）
  - `AudioBus` / `AudioMixer`（Master / Music / SFX / UI；Volume / Mute）
  - `AudioSystem` 单例 + `Engine` 生命周期挂钩
  - `IAudioBackend` 抽象 + **MiniaudioBackend** 首实现
  - Scene / Entity / Component / Asset 卸载的生命周期安全
  - Headless `minEngineTests` suite：`test audio-smoke`
- **Out（明确非目标）:**
  - 复杂 DSP Graph、HRTF、高级环境声学、Wave Propagation、Reverb Zone
  - 类 Wwise 的 Authoring Tool、Sound Cue Graph、Runtime 合成
  - **mp3**（首期；专利/解码复杂度；后续另切片）
  - Editor 专用 Audio 面板、波形预览、Bus 可视化（可另开 ED Feature）
  - Lua ScriptBinding（CORE-F02 之后可选跟进）
  - 与动画 lipsync 深度集成
  - Graphics RHI 体量的音频抽象层（`IAudioBackend` 保持薄接口）

## Reader quick start
1. **§3** — 架构分层、类型与接口草案（审阅重点）。
2. **§3.16–3.18** — 帧时序、所有权、Scene 集成。
3. [Implementation](./AUD-F01_AUDIO_SYSTEM_IMPLEMENTATION.md) — S01–S11 切片。
4. 代码入口（落地后）：`Runtime/Function/Audio/`、`Runtime/Resource/AudioClip.*`。

---

## 1) 背景与目标

### Pain
- 引擎尚无音频子系统；Playground / Demo 无法播放音效与音乐。
- 需要一条与现有 **Asset / Component / System** 模式一致、可展示给求职方的完整链路，而非 demo 级 `PlaySound(path)` 散弹。

### Goals
- **架构可讲清楚**：Asset → Voice → Component → Bus → System → Backend。
- **能跑完整 3D Demo**：多 Entity 同时发声、Listener 跟随相机、Bus 分轨调音量。
- **生命周期可靠**：Entity 销毁、Scene 卸载、Asset 卸载、Engine Shutdown 不悬空。
- **与渲染解耦**：不依赖 RHI / RenderGraph / VK。

### Success
- `minEngineTests.exe test audio-smoke` 覆盖：加载 wav → 2D 播放 → 3D 衰减 → Bus mute → 组件销毁后无泄漏/崩溃。
- Playground 或测试场景可手动听到空间化音效。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| Audio 代码 | **无**（`feat/audio` 仅 worktree 配置） |
| `Asset` / `AssetManager` / `AssetTypeRegistry` | 成熟；`LuaScript`、`EnvironmentMap` 等可照抄 Loader 模式 |
| `Component` / `SceneComponent` / `Transform` | 成熟；`RigidBodyComponent` + `PhysicsBodyId` 可作 Runtime 句柄参考 |
| `Engine` 子系统 | `PhysicsSystem` 单例 + `LogicalTick`；`SceneManager::UnloadActiveScene` 已调 `PhysicsSystem::DestroyWorld` |
| `ObjectManager` 可达性审计 | 有；Audio Voice 须纳入 Shutdown 清理策略 |
| 第三方音频库 | 未 vendoring；首期 miniaudio |

### 对齐的现有模式

| 模式 | 渲染 / 物理 | 音频（本期） |
|------|-------------|--------------|
| 系统单例 | `RenderSystem` / `PhysicsSystem` | `AudioSystem` |
| 底层抽象 | `RHI`（无 `I` 前缀） | **`IAudioBackend`**（已定；薄接口， deliberately 不同于 RHI 命名） |
| Runtime 句柄 | `PhysicsBodyId` | `AudioVoiceId` / `AudioVoiceHandle` |
| Scene 卸载钩子 | `PhysicsSystem::DestroyWorld(scene)` | `AudioSystem::OnSceneUnloaded(scene)` |
| 组件 → 系统 | `RigidBodyComponent` 注册 body | `AudioComponent` 注册 emitter |

---

## 3) 方案

### 3.0 设计原则（实现约束）

1. **先对象关系，再 Backend 调用** — API 形状由 Engine 层定义，Backend 填实现。
2. **Asset ≠ Runtime** — `AudioClip` 不保存 `playing`、当前 `volume` 等实例状态。
3. **场景语义 vs 播放实例** — `AudioComponent` 描述「谁在什么位置、播什么」；`AudioVoice` 是一次播放。
4. **AudioSystem 协调，非 God Object** — 注册表、tick、Bus 汇总在 System；解码/设备在 Backend。
5. **不泄漏第三方类型** — 上层头文件不出现 `ma_*` / `ALuint`；`MiniaudioBackend` 细节仅在 `.cpp`。
6. **适度抽象** — `IAudioBackend` 薄薄一层；不做 Graphics RHI 资源树。

### 3.1 分层架构

```text
┌──────────────────────────────────────────────────────────────────────────┐
│ Layer 4 — Game / Playground / Tests                                       │
│   AudioComponent::Play()   AudioListenerComponent on Camera               │
│   AudioSystem::Play2D / Play3D (convenience, tests)                       │
└────────────────────────────────────┬─────────────────────────────────────┘
                                     │
┌────────────────────────────────────▼─────────────────────────────────────┐
│ Layer 3 — AudioSystem (orchestration)                                     │
│   Voice pool · emitter/listener registry · bus gain · tick sync           │
└───────────────┬──────────────────────────────┬───────────────────────────┘
                │                              │
┌───────────────▼──────────────┐   ┌───────────▼──────────────────────────┐
│ Layer 2a — AudioMixer         │   │ Layer 2b — AudioVoice (runtime inst.) │
│   Bus volume/mute             │   │   state · spatial · backend binding   │
└───────────────┬──────────────┘   └───────────┬──────────────────────────┘
                │                              │
                └──────────────┬───────────────┘
                               ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ Layer 1 — IAudioBackend                                                   │
│   device · voice handles · listener · spatialization                      │
└────────────────────────────────────┬─────────────────────────────────────┘
                                     ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ Layer 0 — MiniaudioBackend (third-party, private)                         │
└──────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────┐
│ Parallel — Asset pipeline (load-time, shared, immutable playback data)    │
│   .wav / .ogg → AudioClipLoader → AudioClip : Asset                       │
└──────────────────────────────────────────────────────────────────────────┘
```

**调用方向：** 仅向下。`AudioComponent` 不直接持有 `IAudioBackend*`；一律经 `AudioSystem`。

### 3.2 模块与目录

```text
minEngine/minEngine/src/Runtime/
  Resource/
    AudioClip.h / .cpp
    Loaders/
      AudioClipLoader.h / .cpp
  Function/Audio/
    AudioTypes.h              // enums, handles, POD structs
    AudioLimits.h               // kMaxVoices, distance clamps
    AudioVoice.h / .cpp         // runtime voice object (engine-owned)
    AudioMixer.h / .cpp
    AudioSystem.h / .cpp
    Components/
      AudioComponent.h / .cpp
      AudioListenerComponent.h / .cpp
    Backend/
      IAudioBackend.h
      AudioBackendTypes.h       // BackendVoiceHandle (opaque)
      MiniaudioBackend.h / .cpp // .cpp only includes miniaudio
  Third-Party/
    miniaudio/
      miniaudio.h
      miniaudio.c               // MINIAUDIO_IMPLEMENTATION in one TU
```

CMake：新增 `miniaudio.c` 到 `minEngine` target；其余模块仅 include `IAudioBackend.h`。

---

### 3.3 类型一览

| 类型 | 层级 | `MEObject`? | 职责 |
|------|------|-------------|------|
| `AudioClip` | Asset | ✓ (`Asset`) | 解码后 PCM / 格式元数据；无播放状态 |
| `AudioClipLoader` | Resource | — | 磁盘 → `AudioClip` |
| `AudioVoiceId` / `AudioVoiceHandle` | Runtime | — | 不透明句柄；查 Voice 池 |
| `AudioVoice` | Runtime | ✗ | 一次播放实例；状态机 + backend 绑定 |
| `AudioBusId` / `AudioMixer` | Runtime | ✗ | Bus 增益 |
| `AudioComponent` | Scene | ✓ | 发射器配置；触发 Play/Stop |
| `AudioListenerComponent` | Scene | ✓ | 听者；Transform → Listener |
| `AudioSystem` | System | — | 单例协调 |
| `IAudioBackend` | Backend | — | 设备与 voice 抽象 |
| `MiniaudioBackend` | Backend | — | miniaudio 实现 |
| `BackendVoiceHandle` | Backend | — | 不透明；仅 System/Backend 内部 |

---

### 3.4 `AudioTypes.h` — 枚举与 POD

```cpp
#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include <cstdint>

namespace minEngine
{
    // --- Handles (align with PhysicsBodyId pattern) ---

    using AudioVoiceId = uint32_t;
    inline constexpr AudioVoiceId InvalidAudioVoiceId = UINT32_MAX;

    struct AudioVoiceHandle
    {
        AudioVoiceId Id{InvalidAudioVoiceId};

        bool IsValid() const { return Id != InvalidAudioVoiceId; }
        bool operator==(const AudioVoiceHandle& other) const { return Id == other.Id; }
        bool operator!=(const AudioVoiceHandle& other) const { return !(*this == other); }
    };

    // --- Playback ---

    ME_ENUM()
    enum class EAudioVoiceState : uint8_t
    {
        Stopped = 0,
        Playing,
        Paused,
    };

    ME_ENUM()
    enum class EAudioBusId : uint8_t
    {
        Master = 0,
        Music,
        SFX,
        UI,
        Count,
    };

    ME_ENUM()
    enum class EAudioAttenuationModel : uint8_t
    {
        Inverse = 0,   // default; maps to miniaudio inverse distance
        Linear,        // optional S07+ if backend supports
        None,          // 2D / full volume regardless of distance
    };

    // --- Spatial (engine-facing; no backend types) ---

    struct AudioSpatialSettings
    {
        bool bSpatialized{true};
        float MinDistance{1.0f};
        float MaxDistance{100.0f};
        EAudioAttenuationModel AttenuationModel{EAudioAttenuationModel::Inverse};
    };

    struct AudioListenerState
    {
        Vector3 Position{};
        Vector3 Forward{0.0f, 0.0f, -1.0f};  // engine convention; normalize on set
        Vector3 Up{0.0f, 1.0f, 0.0f};
        // Vector3 Velocity{};  // reserved for Doppler
    };

    // --- Play request (System internal + test API) ---

    struct AudioPlayParams
    {
        std::shared_ptr<class AudioClip> Clip;
        EAudioBusId Bus{EAudioBusId::SFX};
        float Volume{1.0f};       // linear 0..1 (clamped)
        float Pitch{1.0f};        // 1.0 = normal
        bool bLoop{false};
        AudioSpatialSettings Spatial{};  // bSpatialized=false → 2D
        Vector3 WorldPosition{};         // ignored when !Spatial.bSpatialized
    };

    struct AudioPlayResult
    {
        AudioVoiceHandle Voice{};
        bool bSuccess{false};
        std::string ErrorMessage;  // set when !bSuccess (pool full, clip invalid, etc.)
    };
}
```

**常量（`AudioLimits.h`）：**

```cpp
namespace minEngine
{
    inline constexpr uint32_t kMaxAudioVoices = 32;
    inline constexpr float kMinAudioVolume = 0.0f;
    inline constexpr float kMaxAudioVolume = 1.0f;
    inline constexpr float kMinAudioPitch = 0.25f;
    inline constexpr float kMaxAudioPitch = 4.0f;
    inline constexpr float kDefaultMinDistance = 1.0f;
    inline constexpr float kDefaultMaxDistance = 100.0f;
}
```

---

### 3.5 `AudioClip` — 资产

```cpp
#pragma once
#include "Runtime/Resource/Asset.h"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace minEngine
{
    struct AudioClipFormat
    {
        uint32_t SampleRate{0};
        uint16_t ChannelCount{0};
        uint16_t BitsPerSample{16};  // decoded PCM bits
        uint64_t FrameCount{0};      // per-channel frames
    };

    ME_CLASS()
    class AudioClip : public Asset
    {
        ME_GENERATED_BODY(AudioClip)

    public:
        AudioClip() = default;
        ~AudioClip() override = default;

        bool IsValid() const { return m_FrameCount > 0 && !m_PcmInterleaved.empty(); }

        const AudioClipFormat& GetFormat() const { return m_Format; }
        uint64_t GetFrameCount() const { return m_FrameCount; }

        /** Read-only PCM: interleaved float32 [-1, 1], frames * channels elements. */
        std::span<const float> GetPcmData() const { return m_PcmInterleaved; }

        const std::string& GetSourcePath() const { return m_SourcePath; }

        /** Approximate duration in seconds. */
        float GetDurationSeconds() const;

    protected:
        friend class AudioClipLoader;

        AudioClipFormat m_Format{};
        uint64_t m_FrameCount{0};
        std::vector<float> m_PcmInterleaved;  // decoded at load time
        std::string m_SourcePath;             // project-relative, debug / reload
    };
}
```

**Asset 集成：**

| 项 | 值 |
|----|-----|
| `AssetTypeId` | `"AudioClip"` |
| 扩展名（S02） | `.wav` |
| 扩展名（S02+） | `.ogg` |
| Loader | `AudioClipLoader::Load(const AssetMeta& meta, std::string* outError)` |
| 解码策略 | Loader 用 miniaudio decoder 读入 **float32 interleaved PCM**；Clip 不持有 `ma_decoder` |
| 缓存 | `AssetManager::LoadAsset<AudioClip>(path)` |

**不变量：**
- `AudioClip` 加载后 **immutable**（首期不支持 streaming / 热改 PCM）。
- 多个 Voice 共享同一 `shared_ptr<AudioClip>`，只读 `GetPcmData()`。
- Clip **绝不**存储 `AudioVoiceId`、volume、playing 状态。

---

### 3.6 `AudioVoice` — 播放实例

`AudioVoice` 由 `AudioSystem` 独占分配；**不**暴露为 `MEObject`，避免 GC / 反射干扰。

```cpp
#pragma once
#include "AudioTypes.h"

namespace minEngine
{
    class IAudioBackend;
    class AudioMixer;

    class AudioVoice
    {
    public:
        AudioVoice() = default;

        AudioVoiceId GetId() const { return m_Id; }
        EAudioVoiceState GetState() const { return m_State; }

        bool IsPlaying() const { return m_State == EAudioVoiceState::Playing; }
        bool IsActive() const;  // Playing or Paused

        // --- Control (called by AudioSystem only in MVP; may expose via handle API later) ---

        void Play(IAudioBackend& backend, const AudioMixer& mixer);
        void Stop(IAudioBackend& backend);
        void Pause(IAudioBackend& backend);
        void Resume(IAudioBackend& backend);

        void SetVolume(float volume, IAudioBackend& backend, const AudioMixer& mixer);
        void SetPitch(float pitch, IAudioBackend& backend);
        void SetLoop(bool loop, IAudioBackend& backend);

        void SetWorldPosition(const Vector3& position, IAudioBackend& backend);
        void ApplySpatialSettings(const AudioSpatialSettings& settings, IAudioBackend& backend);

        /** Push effective linear gain to backend (voice * bus * master). */
        void RefreshGain(IAudioBackend& backend, const AudioMixer& mixer);

        /** Called when backend reports natural end (non-loop). */
        void NotifyPlaybackFinished();

    private:
        friend class AudioSystem;

        AudioVoiceId m_Id{InvalidAudioVoiceId};
        std::weak_ptr<AudioClip> m_Clip{};

        EAudioBusId m_Bus{EAudioBusId::SFX};
        float m_Volume{1.0f};
        float m_Pitch{1.0f};
        bool m_bLoop{false};
        EAudioVoiceState m_State{EAudioVoiceState::Stopped};

        AudioSpatialSettings m_Spatial{};
        Vector3 m_WorldPosition{};

        BackendVoiceHandle m_BackendHandle{};  // invalid when stopped
        bool m_bBackendVoiceAllocated{false};

        // Optional back-reference for component-owned voices
        class AudioComponent* m_OwnerComponent{nullptr};
        class Scene* m_OwnerScene{nullptr};
    };
}
```

#### Voice 状态机

```text
                    Play()
         ┌──────────────────────────────┐
         │                              │
         ▼                              │
    ┌─────────┐   Pause()   ┌─────────┐ │ Resume()
    │ Playing │────────────►│ Paused  │─┘
    └────┬────┘◄────────────└────┬────┘
         │         Resume()       │
         │ Stop() / Finished      │ Stop()
         ▼                        ▼
    ┌─────────┐
    │ Stopped │  (backend handle destroyed; slot may be recycled)
    └─────────┘
```

| 转换 | 动作 |
|------|------|
| → Playing | 若未分配 backend voice：`CreateVoice(clip)` → `Play(loop)` → `RefreshGain` |
| → Paused | `backend.Pause` |
| → Stopped | `backend.Stop` → `DestroyVoice` → invalidate handle |
| Natural end (non-loop) | Backend callback 或 `Update` 轮询 → `NotifyPlaybackFinished` → Stopped |

**同一 Clip 多 Voice：** 每次 `Play` 分配新 `AudioVoiceId` + 新 `BackendVoiceHandle`。

---

### 3.7 `AudioMixer` — Bus

```cpp
#pragma once
#include "AudioTypes.h"

namespace minEngine
{
    class AudioMixer
    {
    public:
        AudioMixer();

        float GetBusVolume(EAudioBusId bus) const;
        void SetBusVolume(EAudioBusId bus, float volume);  // clamped 0..1

        bool IsBusMuted(EAudioBusId bus) const;
        void SetBusMuted(EAudioBusId bus, bool muted);

        /** Linear gain for a voice on the given bus (includes Master). Returns 0 if muted. */
        float ComputeEffectiveGain(EAudioBusId bus, float voiceVolume) const;

    private:
        struct BusState
        {
            float Volume{1.0f};
            bool bMuted{false};
        };

        BusState m_Buses[static_cast<size_t>(EAudioBusId::Count)];
    };
}
```

**增益公式（首期）：**

```text
effectiveGain = clamp(voiceVolume, 0, 1)
              * (busMuted ? 0 : busVolume)
              * (masterMuted ? 0 : masterVolume)
```

- `EAudioBusId::Master` 作为全局乘子；Voice 的 `m_Bus` 通常为 Music/SFX/UI，**不**直接设为 Master。
- Bus volume 变更时：`AudioSystem` 遍历 active voices 调 `RefreshGain`。

---

### 3.8 `AudioSystem` — 协调器

模式对齐 `PhysicsSystem`：`Initialize` / `Shutdown` / `Get()` / `HasInstance()`；`Engine` 持有 `shared_ptr<AudioSystem>`。

```cpp
#pragma once
#include "AudioTypes.h"
#include "AudioMixer.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class IAudioBackend;
    class AudioVoice;
    class AudioComponent;
    class AudioListenerComponent;
    class Scene;
    class AudioClip;

    class AudioSystem
    {
    public:
        AudioSystem() = default;
        ~AudioSystem() = default;

        void Initialize();
        void Shutdown();

        static bool HasInstance();
        static AudioSystem& Get();

        void Tick(float deltaTime);

        IAudioBackend* GetBackend() const { return m_Backend.get(); }
        AudioMixer& GetMixer() { return m_Mixer; }
        const AudioMixer& GetMixer() const { return m_Mixer; }

        // --- High-level play API (tests, gameplay, future script) ---

        AudioPlayResult Play(const AudioPlayParams& params);
        AudioPlayResult Play2D(std::shared_ptr<AudioClip> clip,
                               EAudioBusId bus = EAudioBusId::SFX,
                               float volume = 1.0f);
        AudioPlayResult Play3D(std::shared_ptr<AudioClip> clip,
                               const Vector3& worldPosition,
                               const AudioSpatialSettings& spatial = {},
                               EAudioBusId bus = EAudioBusId::SFX,
                               float volume = 1.0f);

        bool StopVoice(AudioVoiceHandle handle);
        bool PauseVoice(AudioVoiceHandle handle);
        bool ResumeVoice(AudioVoiceHandle handle);

        void StopAllVoices();

        // --- Component / scene registration ---

        void RegisterEmitter(AudioComponent* component);
        void UnregisterEmitter(AudioComponent* component);

        void RegisterListener(AudioListenerComponent* listener);
        void UnregisterListener(AudioListenerComponent* listener);

        void OnSceneUnloaded(Scene* scene);

        // --- Voice lookup (optional public for tests) ---

        AudioVoice* FindVoice(AudioVoiceHandle handle);
        const AudioVoice* FindVoice(AudioVoiceHandle handle) const;

        uint32_t GetActiveVoiceCount() const;

    private:
        friend class Engine;
        friend class AudioSmokeTestScope;

        static void SetInstance(AudioSystem* instance);

        AudioVoice* AllocateVoice();
        void FreeVoice(AudioVoice* voice);
        void StopAndFreeVoice(AudioVoice* voice);

        void SyncListenerToBackend();
        void SyncEmittersToBackend();
        void UpdateVoiceStates();

        void ApplyPlayParams(AudioVoice& voice, const AudioPlayParams& params);

        static AudioSystem* s_Instance;

        bool m_Initialized{false};
        std::unique_ptr<IAudioBackend> m_Backend;
        AudioMixer m_Mixer;

        // Voice pool: fixed array or vector of optional<AudioVoice>
        std::vector<std::unique_ptr<AudioVoice>> m_VoiceSlots;
        std::unordered_map<AudioVoiceId, AudioVoice*> m_VoiceById;
        AudioVoiceId m_NextVoiceId{1};

        // Scene registries
        std::vector<AudioComponent*> m_Emitters;
        AudioListenerComponent* m_ActiveListener{nullptr};  // last registered wins

        AudioListenerState m_CachedListener{};
        bool m_bListenerDirty{true};
    };
}
```

#### 内部注册表（实现参考）

```text
m_VoiceSlots[0 .. kMaxAudioVoices-1]   unique_ptr<AudioVoice> or monotonic pool
m_VoiceById: AudioVoiceId → AudioVoice*

m_Emitters: 所有活跃 AudioComponent*（Register on SetOwner / BeginPlay equivalent）
m_ActiveListener: 单个 AudioListenerComponent*（Unregister 时清空或回退）

Per voice (when component-owned):
  voice.m_OwnerComponent
  voice.m_OwnerScene  ← component->GetOwner()->GetScene() or equivalent
```

#### `Tick` 顺序（每逻辑帧）

```text
1. UpdateVoiceStates()        // poll backend: finished → Stopped; recycle slots
2. SyncListenerToBackend()    // if m_bListenerDirty or listener moved
3. SyncEmittersToBackend()    // for each emitter with active voice + spatialized:
                              //   world position from SceneComponent transform
4. m_Backend->Update()        // miniaudio ma_engine update / pump
```

**挂载点：** `Engine::LogicalTick` — `SceneManager::Tick` **之后**，`PhysicsSystem::SimulateActiveScene` **之前**。

```cpp
// Engine::LogicalTick (target)
m_InputSystem->Tick(deltaTime);
m_SceneManager->Tick(deltaTime);
if (m_AudioSystem)
    m_AudioSystem->Tick(deltaTime);
if (m_PhysicsSystem)
    m_PhysicsSystem->SimulateActiveScene(deltaTime);
m_SceneManager->SendAllEndOfFrameUpdates();
```

---

### 3.9 `IAudioBackend` — 底层抽象

**命名已定：** `IAudioBackend`（ deliberately 使用 `I` 前缀 + `Backend` 后缀，与图形 `RHI` 区分；接口保持最小集）。

#### `AudioBackendTypes.h`

```cpp
#pragma once
#include <cstdint>

namespace minEngine
{
    struct BackendVoiceHandle
    {
        uint32_t Index{UINT32_MAX};

        bool IsValid() const { return Index != UINT32_MAX; }
    };
}
```

#### `IAudioBackend.h`

```cpp
#pragma once
#include "AudioBackendTypes.h"
#include "Runtime/Function/Audio/AudioTypes.h"

namespace minEngine
{
    class AudioClip;

    class IAudioBackend
    {
    public:
        virtual ~IAudioBackend() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        /** Pump device / graph; call once per logical frame from main thread. */
        virtual void Update() = 0;

        // --- Voice lifecycle ---

        virtual BackendVoiceHandle CreateVoice(const AudioClip& clip) = 0;
        virtual void DestroyVoice(BackendVoiceHandle handle) = 0;
        virtual bool IsVoicePlaying(BackendVoiceHandle handle) const = 0;

        virtual void PlayVoice(BackendVoiceHandle handle, bool loop) = 0;
        virtual void StopVoice(BackendVoiceHandle handle) = 0;
        virtual void PauseVoice(BackendVoiceHandle handle) = 0;
        virtual void ResumeVoice(BackendVoiceHandle handle) = 0;

        virtual void SetVoiceVolume(BackendVoiceHandle handle, float linearGain) = 0;
        virtual void SetVoicePitch(BackendVoiceHandle handle, float pitch) = 0;

        // --- 3D ---

        virtual void SetListener(const AudioListenerState& listener) = 0;

        virtual void SetVoiceWorldPosition(BackendVoiceHandle handle, const Vector3& worldPosition) = 0;
        virtual void SetVoiceSpatialSettings(BackendVoiceHandle handle,
                                             const AudioSpatialSettings& settings) = 0;

        /** Disable spatialization for 2D UI sounds. */
        virtual void SetVoiceSpatializationEnabled(BackendVoiceHandle handle, bool enabled) = 0;
    };
}
```

**设计说明：**

| 决策 | 理由 |
|------|------|
| `CreateVoice(const AudioClip&)` | Backend 从 Clip PCM 建 `ma_sound`；Clip 无 backend 状态 |
| 分离 `PlayVoice` / `SetVoiceVolume` | 允许先配置再播放；Bus 变更只调 volume |
| `IsVoicePlaying` | 无回调时轮询 natural end |
| 不传 `AudioVoiceId` 到 Backend | Backend 只知 `BackendVoiceHandle`；Engine 映射两层 handle |

#### `MiniaudioBackend`（私有实现要点）

```cpp
// MiniaudioBackend.h — 仅 forward declare ma_engine in .cpp
class MiniaudioBackend final : public IAudioBackend
{
    // ...
private:
    struct BackendVoiceSlot
    {
        BackendVoiceHandle Handle{};
        ma_sound Sound{};           // only in .cpp — header uses pimpl or void* if needed
        bool bAllocated{false};
    };

    ma_engine m_Engine;               // member only in .cpp via unique_ptr impl struct (pimpl recommended)
    std::vector<BackendVoiceSlot> m_Voices;
};
```

**推荐 pimpl：** `MiniaudioBackend` 公开类 + `struct MiniaudioBackendImpl` 在 `.cpp`，避免其他 TU  include miniaudio。

| miniaudio 概念 | Engine 映射 |
|----------------|-------------|
| `ma_engine` | 单实例 per `MiniaudioBackend` |
| `ma_sound` + PCM from memory | 每 `BackendVoiceHandle` |
| `ma_engine_listener` | `SetListener` |
| `ma_sound_set_position` | `SetVoiceWorldPosition` |
| `ma_sound_set_min_distance` / `max` | `SetVoiceSpatialSettings` |

---

### 3.10 `AudioComponent` — 场景发射器

继承 `SceneComponent`（世界坐标来自 `Transform`）。

```cpp
#pragma once
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Audio/AudioTypes.h"

namespace minEngine
{
    class AudioClip;

    ME_CLASS()
    class AudioComponent : public SceneComponent
    {
        ME_GENERATED_BODY(AudioComponent)

    public:
        AudioComponent();
        ~AudioComponent() override;

        void SetOwner(GameObject* inOwner) override;

        ME_FUNCTION(ScriptCallable)
        void Play();

        ME_FUNCTION(ScriptCallable)
        void Stop();

        bool IsPlaying() const;

        std::shared_ptr<AudioClip> GetClip() const { return m_Clip; }
        void SetClip(const std::shared_ptr<AudioClip>& clip) { m_Clip = clip; }

        float GetVolume() const { return m_Volume; }
        void SetVolume(float volume);

        float GetPitch() const { return m_Pitch; }
        void SetPitch(float pitch);

        bool GetLooping() const { return m_bLoop; }
        void SetLooping(bool loop);

        bool GetSpatialized() const { return m_Spatial.bSpatialized; }
        void SetSpatialized(bool spatialized);

        float GetMinDistance() const { return m_Spatial.MinDistance; }
        void SetMinDistance(float distance);

        float GetMaxDistance() const { return m_Spatial.MaxDistance; }
        void SetMaxDistance(float distance);

        EAudioBusId GetBus() const { return m_Bus; }
        void SetBus(EAudioBusId bus) { m_Bus = bus; }

    private:
        friend class AudioSystem;

        void OnTransformUpdated();  // called from System sync path if needed

        ME_PROPERTY(EditAnywhere)
        std::shared_ptr<AudioClip> m_Clip;

        ME_PROPERTY(EditAnywhere)
        float m_Volume{1.0f};

        ME_PROPERTY(EditAnywhere)
        float m_Pitch{1.0f};

        ME_PROPERTY(EditAnywhere)
        bool m_bLoop{false};

        ME_PROPERTY(EditAnywhere)
        bool m_bPlayOnAwake{false};  // S06+; scene load trigger

        ME_PROPERTY(EditAnywhere)
        bool m_bSpatialized{true};

        ME_PROPERTY(EditAnywhere)
        float m_MinDistance{kDefaultMinDistance};

        ME_PROPERTY(EditAnywhere)
        float m_MaxDistance{kDefaultMaxDistance};

        ME_PROPERTY(EditAnywhere)
        EAudioBusId m_Bus{EAudioBusId::SFX};

        ME_PROPERTY(Invisible)
        AudioVoiceHandle m_ActiveVoice{};

        AudioSpatialSettings m_Spatial{};  // assembled from properties in Play()
    };
}
```

**`Play()` 流程：**

```text
AudioComponent::Play()
  → if m_ActiveVoice valid && still playing: optional Stop first (one-shot policy)
  → build AudioPlayParams from m_Clip, m_Volume, transform world pos, m_Spatial, m_Bus
  → AudioSystem::Play(params)
  → store returned handle in m_ActiveVoice
  → link voice.m_OwnerComponent = this
```

**`Stop()` / 析构：**

```text
AudioComponent::Stop() / ~AudioComponent()
  → AudioSystem::StopVoice(m_ActiveVoice)
  → invalidate m_ActiveVoice
  → AudioSystem::UnregisterEmitter(this) on destruction
```

**首期不 Tick：** 位置同步由 `AudioSystem::SyncEmittersToBackend` 批量完成（每帧读 `GetPosition()` 或 world matrix）。

---

### 3.11 `AudioListenerComponent` — 听者

```cpp
#pragma once
#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    ME_CLASS()
    class AudioListenerComponent : public SceneComponent
    {
        ME_GENERATED_BODY(AudioListenerComponent)

    public:
        AudioListenerComponent();
        ~AudioListenerComponent() override;

        void SetOwner(GameObject* inOwner) override;

        /** Override listener orientation from transform (default: use GetForwardVector / GetUpVector). */
        bool GetUseTransformOrientation() const { return m_bUseTransformOrientation; }
        void SetUseTransformOrientation(bool useTransform) { m_bUseTransformOrientation = useTransform; }

    private:
        friend class AudioSystem;

        ME_PROPERTY(EditAnywhere)
        bool m_bUseTransformOrientation{true};
    };
}
```

**Listener 选取规则（已定）：**

- 多个 Listener 时 **最后 `RegisterListener` 的生效**。
- 注册新 Listener 时对前一个 `ME_CORE_WARN`（可选，建议 debug build）。
- `AudioSystem::SyncListenerToBackend` 从 `m_ActiveListener` 的 Transform 构建 `AudioListenerState`。

---

### 3.12 `AudioClipLoader`

```cpp
#pragma once
#include "Runtime/Resource/AssetMeta.h"
#include <memory>
#include <string>

namespace minEngine
{
    class AudioClip;

    class AudioClipLoader
    {
    public:
        static std::shared_ptr<AudioClip> Load(const AssetMeta& meta, std::string* outError = nullptr);

    private:
        static bool DecodeFileToPcm(
            const std::filesystem::path& absolutePath,
            std::vector<float>& outPcmInterleaved,
            AudioClipFormat& outFormat,
            std::string* outError);
    };
}
```

`AssetManager::LoadAssetByMeta` 分流逻辑与 `LuaScriptLoader` 相同：按 `AssetTypeId == "AudioClip"` 调 `AudioClipLoader::Load`。

---

### 3.13 对象所有权与引用关系

```text
AssetManager
  └── shared_ptr<AudioClip>  ─────────────┐
                                          │ shared_ptr (emitters, voices)
GameObject                                │
  ├── AudioComponent ──registers──► AudioSystem::m_Emitters
  │       └── m_Clip ─────────────────────┘
  │       └── m_ActiveVoice ──► AudioVoiceHandle
  └── AudioListenerComponent ──registers──► AudioSystem::m_ActiveListener

AudioSystem
  ├── unique_ptr<IAudioBackend>
  ├── owns AudioVoice[] pool
  │       └── weak_ptr<AudioClip> m_Clip
  │       └── BackendVoiceHandle → MiniaudioBackend slot
  └── does NOT own components (raw ptr; unregister on destroy)

MiniaudioBackend
  └── ma_sound per BackendVoiceHandle (owns playback resource)
```

| 关系 | 策略 |
|------|------|
| Component → System | 原始指针；`Unregister*` 在析构 / `SetOwner(nullptr)` |
| Voice → Clip | `weak_ptr`；Play 时 lock 失败 → 拒绝 |
| Voice → Component | 非拥有 `AudioComponent*`；组件先销毁时 System `StopAndFreeVoice` |
| System → Backend | `unique_ptr<IAudioBackend>` |

---

### 3.14 Scene / Engine 集成

#### SceneManager 钩子

对齐 `PhysicsSystem::DestroyWorld`，在 `UnloadActiveScene` 增加：

```cpp
void SceneManager::UnloadActiveScene()
{
    if (m_CurrentActiveScene)
    {
        if (AudioSystem::HasInstance())
            AudioSystem::Get().OnSceneUnloaded(m_CurrentActiveScene.get());

        if (PhysicsSystem::HasInstance())
            PhysicsSystem::Get().DestroyWorld(m_CurrentActiveScene.get());
    }
    // ... existing reset + GC ...
}
```

`AudioSystem::OnSceneUnloaded(Scene* scene)`：
- 停止所有 `voice.m_OwnerScene == scene` 的 Voice
- 从 `m_Emitters` 移除属于该 scene 的组件（或组件已随 GO 销毁则跳过）
- 若 `m_ActiveListener` 属于该 scene → 清空并 `m_bListenerDirty = true`

#### Engine 启停顺序

```text
StartSystems:
  ... AssetManager ...
  ... InputSystem ...
  AudioSystem::Initialize()     // after WindowSystem if backend needs HWND (miniaudio: optional)
  ... SceneManager ...

ShutdownSystems:
  SceneManager::Shutdown()      // unloads scene → audio voices stopped
  AudioSystem::Shutdown()       // StopAll → backend shutdown
  ... RenderSystem ...
  AssetManager::Shutdown()
```

#### AssetTypeRegistry 登记（S02）

```cpp
RegisterType(AssetTypeDescriptor{
    .AssetTypeId = "AudioClip",
    .RuntimeClassName = GetClassName<AudioClip>(),
    .Extensions = {".wav", ".ogg"},
    .FileDialogFilterLabel = "Audio Clip (*.wav;*.ogg)"});
```

---

### 3.15 3D Audio（首期行为）

| 能力 | Engine API | Backend |
|------|------------|---------|
| Source 世界坐标 | `AudioVoice::SetWorldPosition` | `SetVoiceWorldPosition` |
| Listener | `AudioListenerState` | `SetListener` |
| Min / Max Distance | `AudioSpatialSettings` | miniaudio min/max |
| 衰减 | `EAudioAttenuationModel::Inverse` | 库默认 |
| 2D | `bSpatialized = false` | `SetVoiceSpatializationEnabled(false)` |
| Doppler / HRTF / Reverb | Out | — |

---

### 3.16 生命周期矩阵

| 事件 | 期望行为 |
|------|----------|
| `AudioSystem::Play` | 分配 Voice；`weak_ptr` lock Clip；`CreateVoice` → `PlayVoice` |
| `StopVoice` | Stopped；`DestroyVoice`；回收 `AudioVoiceId` |
| `AudioComponent` 析构 | `Stop` + `UnregisterEmitter` |
| `GameObject` 销毁 | 组件析构链 |
| `Scene::Unload` | `OnSceneUnloaded` 停止 scene voices |
| `AudioClip` 卸载 | 无活跃 Voice 引用后可释放；有活跃则 AssetManager 缓存策略或 System 拒绝新 Play |
| `AudioSystem::Shutdown` | `StopAllVoices` → `m_Backend->Shutdown` |
| Voice 池满 | `Play` 返回 `bSuccess=false`，`ErrorMessage="voice pool exhausted"` |

---

### 3.17 测试策略（`test audio-smoke`）

| 用例 | 验证点 |
|------|--------|
| Load wav fixture | `AudioClip::IsValid()` |
| Play2D + Stop | Backend mock 或短 sleep + `IsVoicePlaying` |
| Bus mute | `SetBusMuted(SFX)` 后 `effectiveGain == 0` |
| Play3D attenuation | Mock backend 记录 `SetVoiceWorldPosition` + distance params |
| Destroy GameObject with AudioComponent | 无 crash；voice count → 0 |
| Scene unload | 同上 |

**Mock backend（建议 S03）：** `class MockAudioBackend : public IAudioBackend` 用于 headless 断言，无需真实音频设备。

---

### 3.18 推荐实现顺序

见 [Implementation Plan](./AUD-F01_AUDIO_SYSTEM_IMPLEMENTATION.md)。MVP Done = S01–S09 + S11。

---

## 4) Backend 选型

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **miniaudio** | 单头、decode + 2D/3D、跨平台 | 高级 DSP 自研 | **首期选用** |
| OpenAL Soft | 经典 3D | 集成重、需预解码 | 备选 ADR |
| SoLoud | 简单 | 3D 弱 | 不选 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Voice 泄漏 | 崩溃 / 爆音 | Handle 表 + Shutdown 顺序 + smoke |
| Clip 卸载竞态 | UAF | `weak_ptr` + Play lock |
| 多 Listener | 不确定 | 最后注册 + WARN |
| 主线程假设 | glitch | 文档 + 首期仅主线程 `Update` |
| Voice 池满 | 静音失败 | 明确错误返回 + log |

---

## 6) 验收标准

- [ ] `AudioClip` 可通过 `AssetManager` 加载 `.wav`（+ `.ogg` 若 S02 包含）
- [ ] `test audio-smoke`：2D、Stop、Bus mute、3D 衰减、组件销毁、Scene 卸载
- [ ] `.\scripts\verify.ps1` 仍绿
- [ ] 公共头文件不 include miniaudio
- [ ] S01–S09 完成 = MVP Done

---

## 7) 已决 / 开放问题

### 已决

| 项 | 决定 |
|----|------|
| 底层抽象命名 | **`IAudioBackend`** + `MiniaudioBackend` |
| 场景组件命名 | **`AudioComponent`**（不用 `AudioSource`） |
| Voice 池上限 | **32** (`kMaxAudioVoices`) |
| 多 Listener | **最后注册生效** + WARN |
| Component Tick | **否**；System 批量同步 Transform |
| Clip 解码时机 | **Loader 时** 解码为 float PCM |

### 待拍板（实现前可选）

| # | 问题 | 建议默认 |
|---|------|----------|
| 1 | `AudioComponent::Play` 时已有 active voice | Stop 旧 voice 再播（one-shot 友好） |
| 2 | Editor Inspector 本期 | 否；反射字段先注册 |
| 3 | `MockAudioBackend` 是否 S03 必做 | 是（headless CI 友好） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Registry + ACTIVE_WORK 登记；Placeholder |
| 2026-08-31 | Design Draft v1（架构、生命周期、miniaudio） |
| 2026-08-31 | Design Draft v2：扩充类型/接口/状态机/集成细节；**IAudioBackend** 命名已定 |
