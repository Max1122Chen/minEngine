# CORE-F17 — LogChannel + Structured LogRecord — Design Spec

## Meta
- **ID:** `CORE-F17`
- **Type:** Feature / Refactor
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-11
- **Related:**
  - [Implementation](./CORE-F17_LOGGING_CHANNELS_IMPLEMENTATION.md)
  - Editor UX: [ED-F09](../../Editor/ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md)（Console 过滤/展示，依赖本 Feature）
  - [ENGINE_0_1_0_ROADMAP.md](../../ENGINE_0_1_0_ROADMAP.md) Phase F1
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - Research notes（非真源）: [docs/external/minEngine LogSystem 重构与 UE 式日志设计.md](../../../external/minEngine%20LogSystem%20重构与%20UE%20式日志设计.md)
  - Code today: `Runtime/Core/Log/LogSystem.*` · `LogConsole.*`
  - UE 参考（只读）: `Logging/LogCategory.h` · `LogMacros.h`
- **Depends on:** 无
- **Blocks:** 各分支可过滤诊断；Editor/Agent 日志数据模型

## TL;DR

**方案：** typed **`LogChannel`**（UE 式 DECLARE/DEFINE，显示名方案 **A**：`LogAsset` → `"Asset"`）+ **`LogRecord`（本期仅 L0）** + **`ILogSink`**；spdlog 仅 Backend。  
**Console：** 存储/展示 **`LogRecord`**，**删除 `LogConsoleEntry` / `LogSource`**。  
**迁移：** **不兼容旧宏**——全仓将 `ME_CORE_*` / `ME_*` / `Get*Logger` 改为 `ME_LOG(LogCore|LogApp, …)`。  
**LogField：** 仅在 `LogRecord` 上 **占位**（空 `fields` / 前向类型），**本期不实现** L1 细节与 `ME_FIELD`。  
**Fatal：** Emit 后 **Flush 所有 Sink，然后崩溃进程**（`std::abort` 或等价；见 §3.2）。  
**Compile-time max：** 模板参数可留，过滤 **先只做 runtime**。

## Scope

### In
- Typed `LogChannel` + 内置 Channel 表（方案 A 显示名）
- `LogSeverity`、`ILogSink`、`LogSystem::Emit`、唯一宏 `ME_LOG`
- `LogRecord`：L0 + **`const LogChannelBase*`** + `channelName` 备份；Field 占位
- `LogConsoleStorage` 存 `LogRecord`；删除 Entry/Source
- 删除旧宏与 `Get*Logger`（全仓硬切）
- Fatal：**Flush + abort**

### Out / 暂缓
- **LogField 实现**（仅占位）
- Log Scope / TLS / MCP Server / Editor 高级 UX（**ED-F09** 负责 Console 过滤产品化）
- Compile-time severity 裁剪（后置）
- UE Display/Log 双轨

## Reader quick start

1. §1 概念 · §3 数据 · §4 接口 · §5 迁移（**硬切**）  
2. §10 已拍板结论  
3. Impl Plan  

---

## 1) 背景与目标

### 1.1 三概念

| 概念 | 含义 | 本期 |
|------|------|------|
| Severity | 严重度 | **做** |
| LogChannel | typed 频道 + 稳定名 | **做** |
| Fields | 结构化上下文 | **占位 only** |

### 1.2 Typed LogChannel（第一期）

- `ME_DECLARE_LOG_CHANNEL_EXTERN` / `ME_DEFINE_LOG_CHANNEL`  
- `ME_LOG(LogAsset, Error, "...")` —— 错误标识符编译失败  
- 显示名 **方案 A**：对象 `LogAsset`，`GetName()` → `"Asset"`  
- 对外 JSON 键：`"channel"`（不用 category）

### 1.3 为何 Console 直接用 LogRecord

`LogConsoleEntry` 与 `LogRecord` 双模型会持续分叉（字段、过滤、Agent）。  
**单一事实来源：** Sink / Storage / UI 都消费 `LogRecord`。

---

## 2) 现状 → 删除清单

| 删除 / 替换 | 动作 |
|-------------|------|
| `ME_CORE_*` / `ME_*` | 全仓改为 `ME_LOG(LogCore/LogApp, Severity, …)` |
| `GetCoreLogger` / `GetClientLogger` | **删除** |
| `LogChannelNames` 仅双名字 | 并入 typed Channel |
| `LogSource` | **删除** |
| `LogConsoleEntry` | **删除**；Storage 存 `LogRecord` |
| `LogLevel::Critical` | 统一 `LogSeverity::Fatal` |
| spdlog 出现在公共调用宏路径 | Backend 实现文件 only |

调用点很多（Engine/Editor/Tests 广泛使用旧宏）——S00 **包含机械替换**，属于本 Feature 工作量，不是「另开兼容层」。

---

## 3) 数据结构设计

### 3.1 LogSeverity

```cpp
enum class LogSeverity : uint8_t
{
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};
```

**过滤约定（拍板进实现）：** 每个 Channel 持有 **最低输出级别** `m_RuntimeSeverity`；仅当  
`messageSeverity >= m_RuntimeSeverity`（数值上 Trace=0 … Fatal=5）时 Emit 下发。  
与 spdlog「level 门槛」同向。

### 3.2 Fatal 行为

打出 `ME_LOG(..., Fatal, ...)` 之后：

1. 各 Sink `Write(record)`  
2. **`Flush()` 所有 Sink**  
3. **`std::abort()`** —— 进程崩溃  

Error = 可恢复、进程继续；Fatal = 不可继续。调试器下 abort 便于当场停下。

### 3.3 LogChannelBase + TLogChannel

```cpp
class MINENGINE_API LogChannelBase
{
public:
    LogChannelBase(const char* name, LogSeverity defaultSeverity, LogSeverity compileTimeMaxSeverity);
    const char* GetName() const;           // e.g. "Asset"
    LogSeverity GetSeverity() const;       // runtime threshold
    void SetSeverity(LogSeverity severity); // clamp to compileTimeMax (even if compile gate unused yet)
    bool IsSuppressed(LogSeverity messageSeverity) const;
    // Register/Unregister with LogSystem from ctor/dtor
};

template <LogSeverity DefaultSeverity, LogSeverity CompileTimeMaxSeverity>
class TLogChannel : public LogChannelBase { /* ctor(name) */ };
```

**CompileTimeMax：** 成员保留；**本期 `IsSuppressed` 只看 runtime threshold**（拍板 3）。

### 3.4 声明宏（方案 A）

```cpp
#define ME_DECLARE_LOG_CHANNEL_EXTERN(ChannelObj, DefaultSev, CompileMax) \
    extern class FLogChannel_##ChannelObj : public ::minEngine::TLogChannel<...> \
    { public: FLogChannel_##ChannelObj(); } ChannelObj

#define ME_DEFINE_LOG_CHANNEL(ChannelObj, NameLiteral) \
    FLogChannel_##ChannelObj::FLogChannel_##ChannelObj() : TLogChannel(NameLiteral) {} \
    FLogChannel_##ChannelObj ChannelObj

// Example:
// ME_DECLARE_LOG_CHANNEL_EXTERN(LogAsset, Info, Trace);
// ME_DEFINE_LOG_CHANNEL(LogAsset, "Asset");
```

内置对象：`LogCore`, `LogApp`, `LogPlatform`, `LogRender`, `LogRHI`, `LogPhysics`, `LogAsset`, `LogSerialization`, `LogAnimation`, `LogUI`, `LogAudio`, `LogScript`, `LogEditor`, `LogTest`（NameLiteral 与对象后缀一致，如 `LogPlatform` → `"Platform"`）。

**调用点归属约定（硬切后二次整理）：** 按模块默认映射——RHI 后端→`LogRHI`；其余 Render→`LogRender`；NFD/FileDialog + GLFW WindowSystem→`LogPlatform`；Resource/Loaders→`LogAsset`；Editor→`LogEditor`；Tests→`LogTest`；Serialization→`LogSerialization`；等。详见 Progress 2026-09-11 条目。

### 3.5 LogRecord（L0 + Channel 引用 + Field 占位）

**Channel 存什么？** 维护者倾向带上完整 Channel，而不是只存 `string`。

| 方案 | 优点 | 缺点 |
|------|------|------|
| 仅 `std::string channelName` | 易序列化、无悬空 | Console/Agent 无法从 Record 再问 Channel 元数据（默认级别、日后颜色等） |
| 仅 `const LogChannelBase*` | 可解析丰富信息；无多余拷贝 | 跨进程/落盘无效；依赖 Channel 静态生命周期 |
| **指针为主 + 名字备份（推荐）** | 进程内富查询；Snapshot/日后 JSON 仍有稳定名 | 多一个字符串字段 |

**结论（本期）：**

```cpp
struct LogSourceLocation
{
    const char* file = nullptr;
    int line = 0;
};

// Placeholder only — L1 暂缓
struct LogField { /* empty / TBD */ };

struct LogRecord
{
    std::chrono::system_clock::time_point timestamp{};
    LogSeverity severity = LogSeverity::Info;

    // Non-owning. Builtin channels are process-lifetime static objects.
    const LogChannelBase* channel = nullptr;

    // Denormalized at Emit from channel->GetName() for text sinks / future serialize
    // and for safety if channel were ever null.
    std::string channelName;

    std::string message;
    LogSourceLocation source{};
    uint32_t threadId = 0;
    std::vector<LogField> fields; // always empty for now

    const char* GetChannelName() const
    {
        if (channel != nullptr)
        {
            return channel->GetName();
        }
        return channelName.c_str();
    }
};
```

- **Editor / 过滤：** 优先 `record.channel`（指针）做按频道勾选、读 `GetSeverity()` 等；展示名用 `GetChannelName()`。  
- **文本 Sink：** 用 `GetChannelName()`。  
- **禁止**在 Record 里存可变 `LogChannelBase&` 引用成员（赋值/进 vector 麻烦）；用指针。  
- Channel 必须是 **静态/永生** 的 DECLARE 对象；禁止临时 Channel 对象指针进 RingBuffer。

### 3.6 ILogSink

```cpp
class ILogSink
{
public:
    virtual ~ILogSink() = default;
    virtual void Write(const LogRecord& record) = 0;
    virtual void Flush() {}
};
```

| Sink | 职责 |
|------|------|
| `SpdlogTextSink` | Record → 文本行 → spdlog/stdout |
| `LogConsoleSink` | `LogConsoleStorage::Push(record)`（存 **LogRecord**） |

### 3.7 LogConsoleStorage

```cpp
class LogConsoleStorage
{
public:
    static void Push(LogRecord record);
    static std::vector<LogRecord> Snapshot();
    static void Clear();
    // max entries ring/truncate as today (e.g. 2000)
};
```

Editor UI：`Snapshot()` 后按 `record.channel` / `record.severity` / `GetChannelName()` 过滤；详见 **ED-F09**。无 `LogConsoleEntry`。

---

## 4) 接口与宏

### 4.1 LogSystem

```cpp
class LogSystem
{
public:
    static void Initialize();
    static void Shutdown();
    static LogSystem& Get();

    static void AddSink(std::shared_ptr<ILogSink> sink);
    static void Emit(const LogChannelBase& channel, LogSeverity severity,
                     std::string message, LogSourceLocation source);
    static void SetChannelSeverity(LogChannelBase& channel, LogSeverity severity);
    static void SetChannelSeverityByName(std::string_view name, LogSeverity severity);
    static void Flush();
    static void RegisterChannel(LogChannelBase& channel);
    static void UnregisterChannel(LogChannelBase& channel);
};
```

Fatal：`Emit` 末尾若 `severity == Fatal` → `Flush()` → **`std::abort()`**。

### 4.2 唯一宏面

```cpp
#define ME_LOG(Channel, Severity, Format, ...) \
    do { \
        if (!(Channel).IsSuppressed(::minEngine::LogSeverity::Severity)) { \
            ::minEngine::LogSystem::Emit( \
                (Channel), \
                ::minEngine::LogSeverity::Severity, \
                /* format */, \
                ::minEngine::LogSourceLocation{__FILE__, __LINE__}); \
        } \
    } while (0)
```

**不提供** `ME_CORE_INFO` / `ME_INFO` 等别名。

### 4.3 生命周期 / 线程

同前版：Channel 静态注册；`Initialize` 装 Sink；Emit 短锁；Console Storage 自带 mutex。

---

## 5) 迁移（硬切，无兼容层）

### 5.1 替换规则

| 旧 | 新 |
|----|-----|
| `ME_CORE_TRACE/DEBUG/INFO/WARN/ERROR(...)` | `ME_LOG(LogCore, Trace/Debug/Info/Warn/Error, ...)` |
| `ME_CORE_CRITICAL(...)` | `ME_LOG(LogCore, Fatal, ...)` |
| `ME_TRACE/.../ERROR(...)` | `ME_LOG(LogApp, …)` |
| `ME_CRITICAL(...)` | `ME_LOG(LogApp, Fatal, …)` |
| `GetCoreLogger()->…` | 改为 `ME_LOG(LogCore, …)`（禁止再暴露 logger） |

子系统文件在改动时可顺便换成更贴切 Channel（如 Render 文件 → `LogRender`）；**S00 最低要求**：全部能编译，Core/App 映射正确。

### 5.2 验证

- 全解编译 Engine + Editor + Tests  
- 故意 `ME_LOG(LogAsest, …)` 应失败  
- Editor 日志面板读 `LogRecord`（**ED-F09** 完善过滤 UX）

---

## 6) Backend

- spdlog 仅实现文件；过滤以 Channel runtime threshold 为准。  
- 倾向单 spdlog logger + 自绘含 channel 的 pattern。

---

## 7) 风险

| 风险 | 缓解 |
|------|------|
| 全仓宏替换量大 | S00 机械替换 + 编译驱动；可脚本辅助 |
| 静态初始化顺序 | Register 只挂指针；Sink 在 Initialize |
| UI 仍假设 LogConsoleEntry | S00/S02 同步改 Editor 读 Record |
| Field 占位误用 | 文档标明禁止填；无 ME_FIELD API |

---

## 8) 验收

- [x] Typed Channel + 方案 A 显示名  
- [x] 仅 `ME_LOG`；旧宏 / Get*Logger / LogConsoleEntry / LogSource **已删除**  
- [x] Storage/UI 使用 `LogRecord`  
- [x] `LogField` 无实现细节（占位）  
- [x] Fatal → **Flush + abort**  
- [x] Runtime 过滤可用；compile-time max 未作为硬门闸  
- [x] `LogRecord` 含 `const LogChannelBase*` + `channelName` 备份  
- [x] Engine/Editor/Tests 构建通过（Editor 最小可编译；过滤 UX 见 ED-F09）  
- [x] Docs 更新  

---

## 9) 切片

| Slice | 内容 |
|-------|------|
| **S00** | 模型 + Channel + Emit + 硬切宏 + Storage=LogRecord；ConsoleWindow **最小可编译**（读 Record） |
| **S01** | 单测 threshold / Snapshot |
| **（ED-F09）** | Console 按 Channel/Severity 过滤、搜索等产品 UX |
| **（后置）** | LogField 真实现 |

---

## 10) 开放点 — 已拍板

| # | 问题 | 结论 |
|---|------|------|
| 1 | 显示名 | **方案 A**（`LogAsset` → `"Asset"`） |
| 2 | Fatal | **Flush + `std::abort`（崩溃）** |
| 3 | Compile-time max | **先 runtime** |
| 4 | 旧 API | **不兼容**；全仓 `ME_LOG` |
| — | LogField | **暂缓实现**；占位 |
| — | LogConsoleEntry | **删除**；存 `LogRecord` |
| — | Record 上 Channel | **`const LogChannelBase*` + `channelName` 备份** |
| — | Editor Console UX | 另登 **ED-F09**（依赖本 Feature 的 Record） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-11 | 二次整理：按模块归属 Channel；新增 `LogPlatform`（NFD + WindowSystem） |
| 2026-09-11 | Fatal→崩溃；LogRecord 持 Channel 指针+name；登记 ED-F09 Editor Console |
| 2026-09-11 | **拍板落地：** 方案 A；旧宏硬切；Field 占位；废除 LogConsoleEntry |
| 2026-09-11 | 初版 → 结构化 → typed Channel 详设 |
