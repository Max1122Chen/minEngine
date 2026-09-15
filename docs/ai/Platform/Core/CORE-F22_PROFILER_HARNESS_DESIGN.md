# CORE-F22 — CPU Profiler Harness — Design Spec

## Meta
- **ID:** `CORE-F22`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/core`
- **Related:**
  - [ENGINE_0_1_0_ROADMAP](../../ENGINE_0_1_0_ROADMAP.md) — Phase P / D7 帧时基线
  - [ENGINE_DESIGN_PHILOSOPHY](../../ENGINE_DESIGN_PHILOSOPHY.md) — Mechanism over Policy；Core 不绑 Editor UI
  - External input（非权威）：`docs/external/minEngine_Profiler_MVP_Design_Prompt.md`
  - 后续 **`ED-*`** Maximum Profiler 面板（本 Feature **不做**）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** 现有 Engine 生命周期（`Engine::Initialize` / `Tick` / `Shutdown`）；无 Job 系统依赖
- **Blocks:** Editor Profiler 接线；未来 Benchmark / Agent 性能查询（复用同一 Session 数据）

## TL;DR

在 `Runtime/Core/Profiling` 落地 **CPU Scope 采集 harness**：Session / Phase / Frame / TLS 事件缓冲 / 延迟分析 / **结构化 Trace 导出** + **只读查询 API**。

- **不只测「帧」：** 用一等公民 **`ProfilePhase`** 覆盖 Startup / Shutdown / 自定义区间；Frame 是 Runtime 循环里可重复的单位。
- **本分支（`feat/core`）：** 采集 + 分析 + 导出 + 查询接口 + Engine 生命周期插桩；**不含** Maximum Timeline / Hierarchy UI。
- **开放点：** §8 已全部按默认拍板。
- **非目标：** GPU、内存、采样式 Profiler、MCP Server、Benchmark Runner。

## Scope

### In（CORE-F22 / `feat/core`）

- `Runtime/Core/Profiling/`：Clock、Name、Event、TLS Buffer、Scope RAII、Session、Collector、Analyzer、Exporter、对外 API / 宏
- **双开关：** 编译期 `ME_ENABLE_PROFILER` + 运行时 `SetEnabled` / Session 录制
- **Phase 模型：** Startup / Runtime / Shutdown / Custom（见 §3.1）
- **Frame：** 仅在 Runtime 路径由 Engine loop 显式 `BeginFrame` / `EndFrame`
- Begin/End 事件配对 → Span；Inclusive / Exclusive；按 Name 聚合（avg / min / max / count；**不做** P50/P95/P99）
- **Chrome Trace Event JSON** 导出（主格式）；可选同 Session 文本摘要
- **只读查询 API**（C++）：供测试、CLI、未来 Editor / Agent 消费（不依赖 ImGui）
- Engine 插桩竖切：`Initialize` / 主 Tick 根 / `Shutdown`（见 §3.6）
- 单测：`test profiler`

### Out / 另轨

| 项 | 归属 |
|----|------|
| Maximum Profiler 面板 / Timeline / Hierarchy 视图 | 后续 **`ED-*`**（消费本 Feature 查询 + Trace） |
| GPU timestamp / 显存 | 后置 Feature |
| CPU 分配器 / 内存追踪 | 后置 |
| Job/Task 生命周期图 | 后置（事件类型可预留） |
| 自动化 Benchmark Runner / 回归库 | 后置（复用 Session） |
| MCP Server / 远程 Profiling | 后置 |
| 完整百分位统计、实时压缩、Perfetto UI | Out |
| 热路径绑定 Chrome Trace schema | **禁止**（导出层适配） |

## Reader quick start

1. §3.1 概念 · **§3.8 数据结构** · **§3.9 完整 API** · §3.10 不变量  
2. §3.11 Chrome Trace 映射 · §3.12 文件布局 · §7 切片  
3. 代码落点（实现后）：`Runtime/Core/Profiling/` · `Engine.cpp`

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 依赖 | Engine 生命周期 **sound**；无现成 Profiling 代码（仅第三方） |
| 债风险 | **Medium** — 若把 Editor UI 塞进 Core 或把「仅 Frame」写死会后悔；Phase 模型一次做对成本低 |
| WIP | F20 Done；本 Feature 钉 `feat/core` |
| 哲学 | 机制（采集/导出/查询）进 Core；展示意见在 Editor — **合** |
| 建议 | **Go with scope cut** — F22 = harness + Trace + query；UI → ED |

---

## 1) 背景与目标

### 1.1 Pain

- 0.1.0 需要可存档的 **性能基线（D7）**，今天没有引擎自有 CPU Scope / Session。
- 只盯「帧」不够：启动卡顿、资源预热、关机顺序问题同样需要区间数据。
- 若先在 Maximum 里做 ImGui 面板，会把采集绑死在 Editor，违反 Runtime/Editor 边界。

### 1.2 Goals

1. 代码中用 RAII Scope 记录嵌套 CPU 区间；关闭时近零开销。
2. **同一套 Session** 能覆盖启动、多帧运行、关机（及自定义区间）。
3. 分析延迟到 Stop / 查询 / 导出路径，不在 Scope 热路径做树构建。
4. 导出 Chrome Trace，可用外部工具查看；C++ 查询 API 可供 Editor 后接线。
5. 架构为多线程 TLS 缓冲留口；MVP 正确性以主线程为准，不引入全局锁事件桶。

### 1.3 Success

- `test profiler` 绿：嵌套 Scope、Phase 边界、Frame 索引、关闭路径不写缓冲、导出文件可解析。
- 一次录制 Session 内可见 `Engine.Startup` / 若干 Frame / `Engine.Shutdown` 区间。
- Core **无** ImGui / Maximum / RHI 依赖。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| 引擎 Profiling | **无**（`src/` 无 ProfileScope） |
| `Engine::Initialize` / `Shutdown` / `Tick*` | 已有明确边界，适合插桩 |
| LogChannel | 已有；Profiler **不**替代日志 |
| Editor | 多文档等并行；本 Feature **不**改 Maximum UI |

输入讨论稿分层 / TLS / Begin–End / Chrome Trace / 禁止事项 **采纳为方向**；「MVP 必须含 Timeline 面板」**收窄为 Out**。

---

## 3) 方案

### 3.1 Phase vs Frame（启动 / 关机也能测）

**结论：能做到。** Frame 不是唯一时间桶。

```text
ProfileSession（一次录制）
├── Phase: Engine.Startup     ← Initialize（可嵌套 Scope；无 Frame）
├── Phase: Engine.Runtime     ← 显式 Phase（拍板：Engine 进入主循环前 Begin）
│   ├── Frame 0
│   ├── Frame 1
│   └── …
└── Phase: Engine.Shutdown    ← Shutdown
```

| 概念 | 含义 | 谁开启/结束 |
|------|------|-------------|
| **Session** | 一次录制窗口 | `StartSession` / `StopSession` |
| **Phase** | Session 内命名区间；可不与帧对齐 | `BeginPhase` / `EndPhase` 或 `ProfilePhaseScope` |
| **Frame** | Runtime 内重复单位；带 `FrameIndex` | `BeginFrame` / `EndFrame` |
| **Scope** | 嵌套 CPU 区间 | `ME_PROFILE_SCOPE` / `ProfileScope` |

**约定 Phase 名（静态字面量）：**

| 名 | 用途 |
|----|------|
| `Engine.Startup` | `Engine::Initialize` |
| `Engine.Runtime` | 主循环存活期（Frame 挂在此 Phase 下） |
| `Engine.Shutdown` | `Engine::Shutdown` |
| 自定义 | 宿主自行 `BeginPhase("Editor.Boot")` 等 |

**规则：**

1. Scope 在任意活跃 Phase（或甚至无 Phase，见 §3.10）内合法；**不要求**先有 Frame。
2. Startup / Shutdown **禁止**调用 `BeginFrame`。
3. 事件携带 `phaseId` + `frameIndex`；非帧时 `frameIndex == kInvalidFrameIndex`（`UINT32_MAX`）。
4. `Profile::Initialize` 在 `Engine::Initialize` **极早**调用（见 §3.6 顺序）。

### 3.2 数据流 / 模块边界

```text
Macros / ProfileScope / Phase / Frame
        ↓
TLS ProfileEventBuffer（只追加 POD 事件）
        ↓  EndFrame / EndPhase / StopSession flush
Session 原始事件存储 + Phase/Frame 元数据
        ↓  StopSession（或显式 Analyze）
Analyzer → Spans + Stats
        ↓
Query API（只读）  ·  ExportChromeTrace
```

**禁止：** Core Profiling 依赖 ImGui、Maximum、OpenGL/Vulkan、Editor 类型。

| 逻辑模块 | 职责 |
|----------|------|
| API / Macros | 对外面；开关；Session/Phase/Frame |
| Collector | Clock、TLS、写事件、flush |
| Session | 录制态、元数据、事件所有权 |
| Analyzer | 配对、树、统计 |
| Exporter | Chrome Trace JSON |
| Query | 只读访问已完成 Session |

### 3.3 宏与启用模型

```text
Compile ME_ENABLE_PROFILER?
  No  → 宏为空；无 Profile 符号依赖（头文件仍可 include，函数可为 stub）
  Yes → Runtime IsEnabled() && IsSessionActive()?
          No  → Scope 构造极早返回（不 Now()、不写缓冲）；drop 计数可选
          Yes → 写 Begin/End
```

**默认（已拍）：**

| 项 | 值 |
|----|-----|
| Debug / RelWithDebInfo | `ME_ENABLE_PROFILER=1` |
| Release | 默认可 `1`（仍靠运行时关）；若体积敏感可 CMake 选项关 |
| 进程启动后 `SetEnabled` | **默认 false**；`StartSession` 时置 true（或要求调用方先 Enable） |
| Engine 自动 Session | **是**（可配置关闭，见 `ProfileSessionDesc` / Engine 配置） |

**宏（公开）：**

```cpp
// ProfileMacros.h — name 仅字面量/静态串；Begin 走导出 C API，Ender 仅存指针
#if ME_ENABLE_PROFILER
  #define ME_PROFILE_SCOPE(name) \
      ::minEngine::Profile::ProfileScopeState ME_PROFILE_CONCAT(_st_, __LINE__); \
      ::minEngine::Profile::ProfileScope_Begin(ME_PROFILE_CONCAT(_st_, __LINE__), (name)); \
      ::minEngine::Profile::ProfileScopeEnder ME_PROFILE_CONCAT(_end_, __LINE__)( \
          &ME_PROFILE_CONCAT(_st_, __LINE__))
  #define ME_PROFILE_FUNCTION() ME_PROFILE_SCOPE(ME_PROFILE_PRETTY_FUNCTION)
  #define ME_PROFILE_PHASE(name) /* 同理：PhaseState + Begin + PhaseEnder */
#else
  #define ME_PROFILE_SCOPE(name)       do { } while (0)
  #define ME_PROFILE_FUNCTION()        do { } while (0)
  #define ME_PROFILE_PHASE(name)       do { } while (0)
#endif
```

`ME_PROFILE_FUNCTION`：**S02 提供**（O3 已拍）。

**跨 DLL / MinGW 注意（已踩坑）：**

- 禁止 `std::function` 跨 DLL（`ForEachSpan` 用头文件模板）。
- Scope **不用** 导出 C++ RAII 类 ctor（MinGW 不可靠）；用 **POD + `ProfileScope_Begin/End` + 本地 Ender**。
- MVP 缓冲为进程内 **`MainBuffer`**（非 TLS map；MinGW DLL `thread_local` 不可靠）。
- `PushEvent` 对 `ProfileEvent` **按 const 引用**传入：GCC 对 ~32B by-value 可能发 `vmovdqa`，Win64 栈只保证 16B 对齐，析构路径易 AV。

---

### 3.8 数据结构（详细）

以下为 **契约形状**；字段名实现时可微调，语义与布局意图应保持。类型均在 `namespace minEngine::Profile`。

#### 3.8.1 标量 ID 与时钟

```cpp
using ProfileTimestamp = uint64_t;   // 单调时钟 tick；单位见 Clock
using ProfileNameId    = uint32_t;   // 0 = invalid
using ProfileThreadId  = uint32_t;   // 稳定会话内 ID（非 OS tid 原样暴露亦可双存）
using ProfilePhaseId   = uint32_t;   // 0 = invalid / none
using ProfileSessionId = uint64_t;   // 单调递增会话号
using ProfileFrameIndex = uint32_t;

constexpr ProfileFrameIndex kInvalidFrameIndex = UINT32_MAX;
constexpr ProfileNameId     kInvalidNameId     = 0;
constexpr ProfilePhaseId    kInvalidPhaseId    = 0;
```

```cpp
struct ProfileClock
{
    static ProfileTimestamp Now();
    static double ToSeconds(ProfileTimestamp delta);
    static double ToMilliseconds(ProfileTimestamp delta);
    static int64_t ToMicroseconds(ProfileTimestamp delta); // Chrome Trace 常用 µs
    // 元数据：来源（QPC / chrono steady_clock）、是否跨线程可比较（MVP: yes, same process）
};
```

#### 3.8.2 Name

```cpp
// 热路径只传 const char* 字面量；Registry 在低频路径分配 Id
struct ProfileNameEntry
{
    const char* Text = nullptr; // 静态存储期
    ProfileNameId Id = kInvalidNameId;
};

class ProfileNameRegistry
{
public:
    // 指针相等优先；否则 strcmp。返回稳定 Id（Session 内或进程内，实现选一并文档化）
    ProfileNameId GetOrRegister(const char* staticLiteral);
    const char* GetText(ProfileNameId id) const;
    size_t GetCount() const;
};
```

**MVP 选择（拍板）：** NameId **进程内稳定**（Registry 活过多个 Session），便于跨 Session 对比；Session 导出时仍写入字符串。

#### 3.8.3 事件（采集层 POD）

```cpp
enum class ProfileEventType : uint8_t
{
    ScopeBegin = 0,
    ScopeEnd   = 1,
    Instant    = 2,  // 预留；MVP 可不产生
    Counter    = 3,  // 预留
    PhaseBegin = 4,  // 可选：亦可用元数据表代替，见下
    PhaseEnd   = 5,
    FrameBegin = 6,
    FrameEnd   = 7,
};

// 紧凑事件：热路径写入。Phase/Frame 边界可用专用事件，或只写元数据表 + Scope 事件带当前 TLS 上下文。
// 推荐 MVP：Scope 事件带 TLS 上下文快照；Phase/Frame 另写元数据记录（不一定进同一 POD 流）。
struct ProfileEvent
{
    ProfileEventType Type = ProfileEventType::ScopeBegin;
    uint8_t  Depth = 0;           // Scope 嵌套深度（Begin 时写入）
    uint16_t _pad = 0;
    ProfileNameId Name = kInvalidNameId;
    ProfileThreadId Thread = 0;
    ProfilePhaseId Phase = kInvalidPhaseId;
    ProfileFrameIndex Frame = kInvalidFrameIndex;
    ProfileTimestamp Timestamp = 0;
};
// 目标：≤ 32 bytes（实现核对对齐）
static_assert(sizeof(ProfileEvent) <= 32);
```

**TLS 写路径上下文（不进事件重复存 OS 名）：**

```cpp
struct ProfileThreadLocalState
{
    ProfileThreadId ThreadId = 0;
    ProfilePhaseId CurrentPhase = kInvalidPhaseId;
    ProfileFrameIndex CurrentFrame = kInvalidFrameIndex;
    uint8_t ScopeDepth = 0;
    // Scope stack 仅用于 Debug 校验 / Depth；正式父子靠 Begin/End 配对恢复
};
```

#### 3.8.4 TLS 缓冲

```cpp
struct ProfileEventChunk
{
    static constexpr size_t kCapacity = 2048; // 可调
    ProfileEvent Events[kCapacity];
    size_t Count = 0;
    ProfileEventChunk* Next = nullptr;
};

class ProfileThreadBuffer
{
public:
    bool TryPush(const ProfileEvent& e); // 满则挂新 chunk 或 drop（见 SessionDesc）
    void Clear();
    // 迭代供 flush
};
```

#### 3.8.5 Phase / Frame 元数据（Session 侧）

```cpp
struct ProfilePhaseRecord
{
    ProfilePhaseId Id = kInvalidPhaseId;
    ProfileNameId Name = kInvalidNameId;
    ProfileTimestamp Start = 0;
    ProfileTimestamp End = 0;       // 未结束则为 0；Stop 时强制收尾
    bool Open = false;
};

struct ProfileFrameRecord
{
    ProfileFrameIndex Index = 0;
    ProfilePhaseId Phase = kInvalidPhaseId; // 通常为 Engine.Runtime
    ProfileTimestamp Start = 0;
    ProfileTimestamp End = 0;
};
```

#### 3.8.6 Session 配置与状态

```cpp
enum class ProfileBufferOverflowPolicy : uint8_t
{
    GrowChunks = 0,  // MVP 默认：继续分配 chunk
    DropNewest = 1,
    DropOldest = 2,  // 后置可实现
};

struct ProfileSessionDesc
{
    const char* Label = "default";           // 静态或短生命周期拷贝到 Session 内 std::string 一次
    uint32_t MaxEventCount = 2'000'000;      // 软上限；超出按 OverflowPolicy
    ProfileBufferOverflowPolicy OverflowPolicy = ProfileBufferOverflowPolicy::GrowChunks;
    bool CaptureThreadNames = true;
};

enum class ProfileSessionState : uint8_t
{
    Idle = 0,
    Recording = 1,
    Analyzing = 2,
    Completed = 3,
};

struct ProfileThreadInfo
{
    ProfileThreadId Id = 0;
    uint64_t OsThreadId = 0;
    std::string Name; // 仅注册路径写入，非热路径
};

struct ProfileSessionStats
{
    uint64_t EventsRecorded = 0;
    uint64_t EventsDropped = 0;
    uint64_t ChunksAllocated = 0;
    uint32_t MaxScopeDepth = 0;
};

struct ProfileSession
{
    ProfileSessionId Id = 0;
    ProfileSessionState State = ProfileSessionState::Idle;
    ProfileSessionDesc Desc{};
    ProfileTimestamp SessionStart = 0;
    ProfileTimestamp SessionEnd = 0;

    std::vector<ProfileThreadInfo> Threads;
    std::vector<ProfilePhaseRecord> Phases;
    std::vector<ProfileFrameRecord> Frames;

    // 原始事件：Stop 后由各 TLS flush 合并（可按 Thread 分桶）
    std::vector<ProfileEvent> Events; // 或 vector<Chunk> 所有权转移

    ProfileSessionStats Stats{};

    // 分析产物（Completed 后有效）
    std::vector<ProfileSpan> Spans;
    ProfileStatsSummary StatsSummary{};

    uint32_t SchemaVersion = 1; // Trace / 查询契约版本
};
```

#### 3.8.7 分析产物

```cpp
struct ProfileSpan
{
    ProfileNameId Name = kInvalidNameId;
    ProfileThreadId Thread = 0;
    ProfilePhaseId Phase = kInvalidPhaseId;
    ProfileFrameIndex Frame = kInvalidFrameIndex;
    ProfileTimestamp Start = 0;
    ProfileTimestamp End = 0;
    ProfileTimestamp Inclusive = 0; // End - Start
    ProfileTimestamp Exclusive = 0; // Inclusive - sum(children Inclusive)（同线程嵌套）
    uint32_t Depth = 0;
    int32_t ParentSpanIndex = -1;   // 在 Spans[] 中的下标；根为 -1
    uint32_t ChildCount = 0;
};

struct ProfileNameAggregate
{
    ProfileNameId Name = kInvalidNameId;
    uint32_t CallCount = 0;
    ProfileTimestamp TotalInclusive = 0;
    ProfileTimestamp TotalExclusive = 0;
    ProfileTimestamp MinInclusive = 0;
    ProfileTimestamp MaxInclusive = 0;
    // Avg = TotalInclusive / CallCount（查询时算，或缓存 double）
};

struct ProfileStatsSummary
{
    std::vector<ProfileNameAggregate> ByName; // 全 Session
    // 可选：按 Phase 过滤的视图由 Query 函数生成，不必预存多份
    ProfileTimestamp SessionDuration = 0;
    uint32_t FrameCount = 0;
    ProfileTimestamp AvgFrameDuration = 0;
    ProfileTimestamp MinFrameDuration = 0;
    ProfileTimestamp MaxFrameDuration = 0;
};
```

**不做（本 Feature）：** P50 / P95 / P99、直方图。

#### 3.8.8 查询视图（只读，可无拥有）

```cpp
struct ProfileSpanFilter
{
    ProfilePhaseId Phase = kInvalidPhaseId;       // invalid = any
    ProfileFrameIndex Frame = kInvalidFrameIndex; // invalid = any；或另设 bool HasFrameConstraint
    ProfileThreadId Thread = 0;                   // 0 = any（若 0 保留为 valid，则用 optional）
    ProfileNameId Name = kInvalidNameId;          // invalid = any
};

// 实现可用 optional 或 sentinel 文档约定；推荐 Query 用显式 optional 参数避免 ThreadId 0 歧义。
```

---

### 3.9 完整 API（详细）

全部在 `namespace minEngine::Profile`。头文件建议 `Profile.h` 聚合公开 API；内部类可分散。

#### 3.9.1 生命周期与开关

```cpp
void Initialize();   // 幂等；创建全局 NameRegistry、主线程 TLS 槽
void Shutdown();     // 若仍 Recording → 强制 StopSession；释放缓冲

void SetEnabled(bool enabled);
bool IsEnabled();    // 编译关闭时恒 false

bool IsCompileEnabled(); // constexpr 或返回 ME_ENABLE_PROFILER
```

#### 3.9.2 Session

```cpp
ProfileSessionId StartSession(const ProfileSessionDesc& desc = {});
// 若已 Recording：Debug assert / 返回 0；Release 忽略或 Stop 再 Start（MVP：拒绝并打日志）

void StopSession();
// flush 所有已注册线程缓冲 → Analyze → State=Completed → 成为 LastCompleted

bool IsSessionActive();
ProfileSessionId GetActiveSessionId(); // 无则 0

// 录制中：仅允许读元数据/Stats 计数；禁止依赖 Spans
const ProfileSession* GetActiveSession();

// Stop 后快照；下一次 Start 可清空或移入 ring（MVP：保留 1 个 LastCompleted，Start 时覆盖）
const ProfileSession* GetLastCompletedSession();

uint64_t GetDroppedEventCount(); // 进程级或当前 Session；便于测试 O2
```

**Engine 自动 Start（O1 已拍）：**

```cpp
// Engine::Initialize 极早：
Profile::Initialize();
Profile::SetEnabled(true);
ProfileSessionDesc desc;
desc.Label = "Engine";
Profile::StartSession(desc);
Profile::BeginPhase("Engine.Startup");
// … 子系统 …
Profile::EndPhase();

// 进入主循环前：
Profile::BeginPhase("Engine.Runtime");

// 每帧 TickOneFrame：
Profile::BeginFrame();
// …
Profile::EndFrame();

// Shutdown：
Profile::EndPhase(); // Runtime
Profile::BeginPhase("Engine.Shutdown");
// …
Profile::EndPhase();
Profile::StopSession(); // 或 Shutdown() 内 Stop
Profile::SetEnabled(false);
```

测试 / 无 Engine 场景：**显式** `StartSession`，不依赖 Engine 自动逻辑。

可选：`EngineConfig` / 命令行 `--profile=0` 关闭自动 Session（S04 实现时接线；Design 预留）。

#### 3.9.3 Phase / Frame

```cpp
void BeginPhase(const char* staticName);
void EndPhase();
// 嵌套 Phase：MVP **不支持**（栈深 1）。需要子区间用 Scope。
// 错误：未 Begin 就 End → 日志 + no-op

void BeginFrame();
void EndFrame();
// 要求：当前 Phase 已 Begin（推荐 Engine.Runtime）。
// FrameIndex 从 0 递增；未 BeginFrame 时 Scope 的 Frame=kInvalidFrameIndex
```

```cpp
class ProfilePhaseScope
{
public:
    explicit ProfilePhaseScope(const char* staticName);
    ~ProfilePhaseScope();
    ProfilePhaseScope(const ProfilePhaseScope&) = delete;
    ProfilePhaseScope& operator=(const ProfilePhaseScope&) = delete;
};
```

#### 3.9.4 Scope

```cpp
class ProfileScope
{
public:
    explicit ProfileScope(const char* staticLiteralName);
    ~ProfileScope();
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
};
```

构造：`!IsEnabled() || !IsSessionActive()` → 置 `m_Active=false` 并 return。  
否则：`GetOrRegister`（可缓存线程本地最近 Name——优化后置）、`Now()`、push `ScopeBegin`、`Depth++`。  
析构：对称 `ScopeEnd`。

#### 3.9.5 分析（通常由 StopSession 调用）

```cpp
// 可对 Completed 或已 flush 的 Recording 快照调用；公开以便测试
bool AnalyzeSession(ProfileSession& session);
```

算法要点（同线程）：

1. 按 Timestamp 稳定排序（若多缓冲合并后无序）。
2. 每线程栈配对 Begin/End → `ProfileSpan`。
3. Exclusive = Inclusive − Σ children.Inclusive（仅统计同父直接子）。
4. 按 `NameId` 聚合 → `ProfileNameAggregate`。
5. 由 `Frames[]` 填 Frame Duration 统计。

跨线程父子：**不做**；各线程独立树。

#### 3.9.6 导出

```cpp
struct ProfileExportDesc
{
    bool PrettyPrint = false;
    bool IncludeStatsSummaryAsMetadata = true; // 写入 chrome trace metadata / 旁路 JSON 字段
};

bool ExportChromeTrace(
    const ProfileSession& session,
    const std::filesystem::path& path,
    const ProfileExportDesc& desc = {});

// 可选 S03：写纯文本摘要（avg/min/max 表）
bool ExportStatsText(const ProfileSession& session, const std::filesystem::path& path);
```

要求：`session.State == Completed`（或至少已 Analyze）。失败返回 false + 日志。

#### 3.9.7 查询 API（Editor / 测试消费面）

```cpp
const ProfileStatsSummary* GetStatsSummary(const ProfileSession& session);

// Spans：返回指向 session.Spans 的视图；filter 在实现内线性扫描（MVP）
void ForEachSpan(
    const ProfileSession& session,
    const ProfileSpanFilter& filter,
    const std::function<void(const ProfileSpan&)>& fn);

const ProfileNameAggregate* FindNameAggregate(
    const ProfileSession& session,
    ProfileNameId name);

const ProfileNameAggregate* FindNameAggregate(
    const ProfileSession& session,
    const char* staticName);

const ProfilePhaseRecord* FindPhase(const ProfileSession& session, const char* staticName);
const ProfileFrameRecord* FindFrame(const ProfileSession& session, ProfileFrameIndex index);

size_t GetSpanCount(const ProfileSession& session);
const ProfileSpan* GetSpan(const ProfileSession& session, size_t index);

// 名称
const char* GetNameText(ProfileNameId id);
```

**稳定性约定（给未来 ED-*）：**

- `GetLastCompletedSession()` 在下一次 `StartSession` 前指针稳定。
- `Spans` / `Events` 在 Completed 后只读；调用方不得长期悬挂跨 `StartSession`。
- 不提供可变引用修改 Span 树。
- **`ForEachSpan` 为头文件模板**（禁止 `std::function` 跨 DLL 边界）。

---

### 3.10 不变量与错误行为

| 情况 | 行为 |
|------|------|
| `ME_ENABLE_PROFILER=0` | 宏 no-op；API 可 stub |
| Enabled 但无 Session | Scope/Phase/Frame **静默 no-op**；`EventsDropped` 或独立 `noopHits` 计数（O2） |
| Scope 跨 Phase 边界未闭合 | 允许（Phase End 不强制关 Scope）；Analyze 按线程栈仍配对；**推荐** RAII 不跨 Phase |
| Phase 嵌套 | MVP 拒绝 / 断言；用 Scope 表达子区间 |
| BeginFrame 在 Startup | 日志 Warning + no-op 或仍产生 Frame（**推荐 no-op + Warning**） |
| 未 EndFrame 就 Stop | 自动 EndFrame |
| 未 EndPhase 就 Stop | 自动 EndPhase |
| Begin/End Scope 不平衡（手动破坏） | Analyze 丢弃残栈并记 `UnbalancedScopes`（Stats 扩展字段） |

---

### 3.11 Chrome Trace 映射

| 内部 | Chrome Trace |
|------|----------------|
| ScopeBegin / ScopeEnd | `ph":"B"` / `"E"`，或合并为 `"X"`（dur）—— **Exporter 可选 X 以减小文件**；MVP 推荐导出 **X**（由 Span 生成） |
| Thread | `tid`；`ThreadInfo.Name` → metadata `thread_name` |
| Phase 名 | Span `name` 保持 Scope 名；Phase 用 **即时事件** `"ph":"i"` + name `Phase:Engine.Startup` 或 metadata |
| Frame 边界 | Instant `Frame %u` 或 Counter；Frame Duration 可由连续 FrameBegin/End 推出 |
| 时间单位 | `displayTimeUnit": "ms"`；事件 `ts`/`dur` 用 **微秒** |
| 旁路 | 根对象可含 `minEngine`: `{ schemaVersion, sessionId, engineVersion, stats }`（非标准扩展；工具忽略未知字段） |

导出 **不读** TLS；只读 `ProfileSession` 分析后数据。

---

### 3.12 源文件布局（建议）

避免过度拆分；允许合并：

```text
Runtime/Core/Profiling/
  Profile.h                 // 公开 API 入口
  ProfileTypes.h            // 枚举、POD、Desc、Span、Summary
  ProfileMacros.h
  ProfileClock.h/.cpp
  ProfileNameRegistry.h/.cpp
  ProfileScope.h            // ProfileScope + ProfilePhaseScope inline 可
  ProfileCollector.h/.cpp   // TLS buffer、push、flush
  ProfileSession.h/.cpp     // Start/Stop、状态
  ProfileAnalyzer.h/.cpp
  ProfileExporter.h/.cpp    // Chrome Trace
  ProfileQuery.h/.cpp       // 或并入 ProfileSession.cpp
```

CMake：并入 `minEngine` 目标现有 Core 源 glob（若项目用 GLOB）。

---

### 3.4 Session 合并与缓冲策略（MVP）

- 每线程 TLS chunked buffer；默认 `GrowChunks`，受 `MaxEventCount` 约束，超出则 drop + 计数。
- **Flush 时机：** `EndFrame`、`EndPhase`、`StopSession`；主线程必做；其它线程在 Stop 时汇集（注册表列出 buffer）。
- 线程注册：首次写事件时注册进 Session `Threads[]`。
- 跨线程时间戳：同一 `ProfileClock`，可同轴显示。

### 3.5 导出格式（摘要）

见 §3.11。主格式 Chrome Trace JSON。查询不经文件。

### 3.6 Engine 插桩竖切

| 顺序 | 动作 |
|------|------|
| 1 | `Profile::Initialize`（早于或紧随 Log 初始化） |
| 2 | `SetEnabled(true)` + `StartSession`（可用配置关闭） |
| 3 | `BeginPhase("Engine.Startup")` … 子系统 Init Scope … `EndPhase` |
| 4 | `BeginPhase("Engine.Runtime")` |
| 5 | 每帧 `BeginFrame` / `LogicalTick`+`RendererTick` Scope / `EndFrame` |
| 6 | `EndPhase` Runtime → `BeginPhase("Engine.Shutdown")` … `EndPhase` |
| 7 | `StopSession` |

最小 Scope 名示例：`Engine.Initialize`、`Engine.StartSystems`、`Engine.LogicalTick`、`Engine.RendererTick`、`Engine.ShutdownSystems`（实现时可增减）。

### 3.7 Editor / Agent 预留

稳定面：`GetLastCompletedSession` + `ForEachSpan` / `GetStatsSummary` + `ExportChromeTrace`。  
MCP schema **不**在本 Feature 定义。

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| Phase + Frame | **选用** |
| 仅 Frame / 仅 Scope | 否作主模型 |
| 本 Feature 含 Editor UI | **否** |
| Chrome Trace | **选用** |
| 字面量 + NameId | **选用** |
| 命名空间 `minEngine::Profile` | **选用（O6）** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Init 过早未 Initialize | 丢启动数据 | §3.6 顺序；单测覆盖 Startup Phase |
| 无 Session 丢事件 | 静默 | drop 计数；测试断言 |
| 启用拖慢帧 | 基线失真 | 关闭路径极简；禁热路径锁/string |
| Phase/Frame 误用 | 数据脏 | Warning + no-op；单测 |
| Editor 耦合 | 边界破 | Review 禁 ImGui include |
| `sizeof(ProfileEvent)` 超标 | 缓存差 | static_assert；削字段 |
| MinGW AVX `vmovdqa` + 16B 栈对齐 | Scope End AV | `PushEvent` const-ref；勿按值传 32B POD |

---

## 6) 验收标准

### 功能

- [x] 嵌套 Scope：Depth、父子、Inclusive/Exclusive 正确
- [x] Startup Phase 内 Scope **无需** Frame
- [x] Runtime FrameIndex 递增
- [x] Shutdown Phase 可录制（Engine `Engine.Shutdown` 插桩；suite 覆盖 Startup/Runtime，Shutdown 靠集成路径）
- [x] 无 Session：Scope no-op + NoopHits API（`GetNoopHitCount`；热路径静默）
- [x] `ExportChromeTrace` 写出含 `"ph":"X"` 的文件（Chrome/Perfetto 人工打开可选）
- [x] Query：按 Name 取 count；父子/Inclusive 断言覆盖
- [x] Core 无 ImGui / Maximum 依赖

### 工程

- [x] `minEngineTests.exe test profiler` PASS（5/5 smoke）
- [x] Registry / ACTIVE_WORK / Progress；本 Design 勾选（S05）

### 非目标

- [x] 无 GPU / 内存 / Editor Timeline 进入本 Feature

---

## 7) 实施切片

| Slice | 内容 | 验收 |
|-------|------|------|
| **S01** | Types、Clock、Registry、TLS、Scope、Session Start/Stop、基本 Analyze 配对 | 嵌套 Duration/Depth 单测 |
| **S02** | Phase、Frame、Exclusive、按名聚合、`ME_PROFILE_FUNCTION` | Startup 无 Frame；多 Frame；聚合正确 |
| **S03** | Chrome Trace + Query API + 可选 Stats 文本 | 文件可开；Query 与 Summary 一致 |
| **S04** | Engine 自动 Session + 三阶段插桩 | 一次 Session 含 Startup/Frames/Shutdown |
| **S05** | 上限/drop、关闭路径、文档 Done | DoD |

---

## 8) 开放点（已拍板）

| # | 决策 |
|---|------|
| O1 | Engine **可配置自动** `StartSession`；测试显式 Start |
| O2 | 无 Session → Scope **静默 no-op** + drop/noop 计数 |
| O3 | `ME_PROFILE_FUNCTION` 在 **S02** |
| O4 | **不做**百分位 |
| O5 | `ExportChromeTrace(session, path)`；CLI 后置 |
| O6 | 命名空间 **`minEngine::Profile`** |
| O7 | Editor UI 另登 **`ED-*`** |

---

## 9) Status note

- **Done（2026-09-15）。** S01–S05 收口；`test profiler` 5/5 PASS。
- 实现偏离：MainBuffer（非 TLS）；宏为 POD+Ender；`PushEvent` const-ref（MinGW AVX）。
- Editor Timeline / Hierarchy → 另登 **`ED-*`**。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | 初稿：Phase+Frame；无 Editor UI |
| 2026-09-15 | 实现 S01–S04；MinGW AVX/`std::function`/TLS 踩坑记入 §3.7；验收部分勾选 |
| 2026-09-15 | 开放点全部按默认拍板；Status → **Planned**；增补 §3.8 数据结构、§3.9 API、§3.10–3.12 |
| 2026-09-15 | S05 收口；Status → **Done**；`test profiler` 5/5 |
