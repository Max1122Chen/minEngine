# CORE-F41 Session Log File and Readback — Design Spec

## Meta
- **ID:** `CORE-F41`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `Review`
- **Owner:** project maintainer
- **Last updated:** 2026-10-19
- **Related:**
  - minCraft feedback: `minCraft/docs/ai/INFRA/ENGINE_FEEDBACK_PROMPT_LOG_FILE_AND_EXPORT.md`（stdio 丢弃后无法回读）
  - [ED-F09](../../Editor/ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md)（Console 已有过滤与 Copy Visible）
  - [MCP-F02](../../Editor/MCP/MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md) · [MCP-F03](../../Editor/MCP/MCP-F03_MCP_SERVER_DESIGN.md)（`editor_query` 透传 Host op，不在 TS 里建目录）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `LogSystem` / `ILogSink` / `LogConsoleStorage`；`ProjectManager::OpenProject`；`CapabilityRegistry` + `editor_query`
- **Blocks:** 无
- **Does not block:** `ASSET-F10`；`UI-F08`；`CORE-F38`

## TL;DR

Editor / Game 的 `ME_LOG` 只进 stdout 和 2000 条内存环。stdio 被丢掉、环被刷掉或进程退出后，Info/Warn 无法回读。本 Feature 在 `LogSystem::Initialize` 增加**每进程一个会话文件**（Warn 立即刷、Info 周期刷），项目打开后写一条指针文件；Automation 用已有 `editor_query` 读环的尾部并返回文件路径。Console 的整表复制已经按行拼接，补上单条复制和 `Ctrl+C`。

## Scope
- **In:** 默认文件 sink；刷盘策略；项目侧会话指针；`editor.get_log`；Console 单条复制与快捷键。
- **Out:** 把 Console 做成可选中文本编辑器；用 MCP 回传整份日志文件；按大小滚动的通用日志产品；改通道过滤语义。

## Reader quick start
1. 先读 **Problem** 与 **Current Architecture**（复制按钮实际做什么）。
2. 再读 **Proposed Architecture**、**API**、**Failure Model**。
3. 验收看 **Verification Strategy**。实现前走 design-review。

## Pre-flight

| 项 | 结论 |
|----|------|
| **Complexity** | **L2**（路径与刷盘是日志基础契约；Automation 沿用现有 query；Console 是小 UX） |
| **Prerequisites** | `ILogSink`、环、`editor_query`、`OpenProject` **sound** |
| **Tech-debt risk** | **low**（一个文件 sink + 一个 query；不新增日志框架） |
| **WIP** | 工作区有未提交的 `UI-F08`。本 Design 可先落文档；**编码等到那批提交或暂存之后**。不升 Primary（`ASSET-F10` 仍是主线） |
| **Philosophy** | 落盘与只读快照是机制，进 Core / 现有 Automation。不规定游戏该记哪些业务事件 |
| **Recommendation** | **Go with scope cut**：S01 文件 + 指针；S02 `editor.get_log`；S03 Console 单条复制。不做磁盘回放进 MCP |

---

## Problem

1. **现场只活在进程里。** `LogSystem::Initialize`（`Engine::Initialize` 最早几步，早于 `PathRegistry` 和 `OpenProject`）只挂 `stdout_color_sink` 和 `LogConsoleSink`。`flush_on` 仅为 Error。`LogConsoleStorage::kMaxEntries` 为 2000。关进程、stdio 被 ignore、或环被挤掉之后，Info/Warn 不在磁盘上。
2. **自动化读不到。** `Snapshot` / `CopyInto` 已有，但没有 `editor.get_log`。MCP 的 `editor_query` 只能调用 Host 已登记的 op。
3. **编辑器复制手感。** 见下节：整表复制**已经换行拼接**；没有单条操作。鼠标拖选跨不过独立的 `TextColored` 行，容易看成「只能复制一条」。

## Requirements

### Must（S01 — 会话文件）

1. **每个进程一个文件。** `LogSystem::Initialize` 成功后，后续通过 `LogSystem` 的记录（含 `"LogSystem Initialized"`）写入该文件。`Shutdown` 在拆 sink 之前 `Flush()`（现有顺序保持）。
2. **路径。** 目录是**进程可执行文件**所在目录下的 `Logs/`（`GetModuleFileNameW(nullptr)` 一类的进程 exe，不是 DLL；不用 `current_path()`）。文件名 `minEngine-YYYYMMDD-HHMMSS-mmm-<pid>.log`（毫秒避免同一秒内 Shutdown 再 Initialize 覆盖）。目录不存在则创建。
3. **行内容。** 每条一行，本地时间，无时区后缀：`[YYYY-MM-DD HH:MM:SS.mmm] [Channel] [Severity] message`。有源位置时再追加 ` (file:line)`。与 Console 剪贴板格式不必逐字相同。
4. **刷盘。** 文件 logger：Warn 及以上立即 flush。Trace/Debug/Info 至少每 1 秒 flush 一次（允许用 spdlog 的周期 flush 线程，或引擎帧里调用 `LogSystem::Flush()`；实现选一种并在测例里钉死「不调用显式 Flush 时，Warn 已在文件中」）。stdout logger 保持 Error 才 `flush_on`。
5. **启动清旧文件。** 打开新会话前，同一 `Logs/` 目录里只保留最近 **20** 个 `minEngine-*.log`（按修改时间），更旧的删除。删除失败不阻止本次启动。
6. **项目指针。** `ProjectManager::OpenProject` 在 `SetProjectRoots` 成功之后，写 `<ProjectRoot>/Saved/Logs/latest-session-<exe-stem>.txt`（stem 来自进程 exe 文件名，例如 Editor 与 Game 各写各的，后打开的进程不覆盖另一个）。内容一行：当前会话文件的绝对路径（`path.string()`，与仓库里其它路径字符串一致）。这是指针，不是第二份日志。没有会话文件时不写指针。`CloseCurrentProject` 不删除会话文件，也不删指针。
7. **查询路径。** Core 提供当前会话文件绝对路径的只读访问（空表示本进程没有打开文件 sink）。

### Must（S02 — Automation 读环）

8. **Op：** `editor.get_log`，kind `query`，**不**要求 `sessionId`。登记进 `CapabilityRegistry`，handler 走 `EditorAutomationSurface::RegisterHandlers`（与 `editor.get_state` 相同）。
9. **MCP 不新增专用 tool。** 调用方使用现有 `editor_query`：`domain=editor`，`action=get_log`。Host `editor.get_capabilities` 会列出该 op。
10. **Payload：**
    - `limit`：可选整数。缺省 **200**。钳制到 **1…500**。
    - `minSeverity`：可选字符串，取值 `Trace|Debug|Info|Warn|Error|Fatal`。缺省 `Trace`（不过滤）。非法值 → 错误响应，不返回部分结果。
11. **Result：**
    - `logFile`：绝对路径；没有文件 sink 时为 `""`。
    - `sessionPointer`：若当前项目已写出指针文件则为其绝对路径，否则 `""`。
    - `ringTruncated`：自上次 `LogConsoleStorage::Clear`（若从未 Clear，则自本次 `Initialize` 之后第一次 Push）以来，环是否已经丢掉过更早的记录。`Clear` 把它清回 false。
    - `entries`：环中**时间正序**的尾部，长度 ≤ `limit`，且 severity 不低于 `minSeverity`（枚举序 Trace < Debug < Info < Warn < Error < Fatal）。每条含 `timestamp`（与文件行相同的 `YYYY-MM-DD HH:MM:SS.mmm` 本地时间）、`channel`、`severity`、`message`、`sourceFile`（无则 `""`）、`sourceLine`（无则 `0`）。
12. **不读磁盘文件进响应。** 环里没有的更早记录，调用方自己打开 `logFile`。

### Must（S03 — Console 复制）

13. **保留现有整表复制语义。** Output 页按钮把**当前过滤后的行**（含 Collapse 的 `(xN)`，含不在滚动窗口里的行）用 `\n` 拼成一段文本，再 `SetClipboardText`。按钮文案改为 **Copy Visible**，tooltip 说明是过滤结果而不是磁盘全文。
14. **单条。** 行上右键 **Copy Line**，剪贴板只有这一行，格式与整表里的对应行相同（末尾换行可有可无，整表与单条规则一致）。
15. **`Ctrl+C`。** Console 列表区域 hovered 或 focused，且键盘焦点不在搜索框 / 其它 `InputText` 上时，等价于 Copy Visible。搜索框拥有焦点时，`Ctrl+C` 仍交给 ImGui 文本编辑。

### Should

16. 复制成功后状态行短暂显示 `Copied N lines`（N 为写入剪贴板的行数）。
17. 菜单项 **Show Log File**：用系统文件管理器打开会话文件所在目录（文件尚未创建时禁用）。

### Non-Goals

- 解析并回传整个 `.log`（体积和协议都不适合 query）。
- 把内存环改成无上限，或让 Console 显示磁盘历史。
- 多进程合并进同一个文件、按日期的单一 rolling 文件、远程上传。
- 重做 Channel / Severity / 搜索 / Collapse（ED-F09 已有）。
- 游戏业务日志分类、采样、或「只记录 Error」的产品策略。

## Constraints

- `LogSystem::Initialize` 早于项目根目录可用，会话文件不能依赖 `ProjectRoot`。
- 日志写入在任意调用 `ME_LOG` 的线程上发生；sink 必须可并发写（现有 `AddSink` 在锁内调用 `Write`）。
- Info 路径不能每条都 flush（材质编译一类风暴）。
- 新 Automation op 必须同时进 CapabilityRegistry 和 handler 表（现有 `AssertHandlersCoverCapabilities`）。
- 前向开发：不保留「无文件 sink」的配置开关，除非测试需要把目录指到临时文件夹。

## Preferences

- 复用 spdlog 文件 sink，不手写缓冲。
- 指针文件用纯文本一行路径，方便人和脚本打开。

## Assumptions

- Windows 上可用模块路径 API 得到 exe 目录（Editor / Game / Tests 都适用）。其它平台若尚未有对应实现，S01 在该平台回退到「跳过文件 sink 并打一条 stdout 警告」即可，不阻塞 Windows 验收。
- 测试进程调用 `LogSystem::Initialize()` 时也会创建会话文件。默认落在测试 exe 的 `Logs/` 可以接受；**新测例**必须把目录指到临时目录，避免断言依赖构建树里的历史文件。

## Current Architecture

```text
ME_LOG → LogSystem::Emit
           → SpdlogTextSink (stdout, flush_on err)
           → LogConsoleSink → LogConsoleStorage ring (2000)
Editor ConsoleWindow
           → CopyInto on generation change
           → filter / collapse → ImGui rows
           → Button "Copy" builds one string with '\n' per filtered row
Automation
           → editor_query → Host op（无 get_log）
```

**复制事实（2026-10-19 对照 `ConsoleWindow::DrawOutputTab`）：**

- 按钮路径在 clipper 之外遍历全部 `drawIndices`，每行末尾追加 `\n`。不是「没换行所以粘成一条」。
- 复制范围是**过滤后的行**，不是环里未通过过滤的行，也不是磁盘。
- 行控件是 `Selectable` + `TextColored`。没有右键菜单，没有行选择，没有 `Ctrl+C`。拖选不能跨行。

## Proposed Architecture

```text
Initialize
  → ensure <exe>/Logs
  → prune to 20 session files
  → open minEngine-<stamp>-<pid>.log
  → stdout sink (flush_on err) + file sink (flush_on warn + ≤1s) + console ring

OpenProject success
  → write <ProjectRoot>/Saved/Logs/latest-session-<exe-stem>.txt  (one absolute path)

editor.get_log
  → tail of ring (+ filters) + logFile + sessionPointer + ringTruncated

Console
  → Copy Visible / Ctrl+C = filtered rows joined by newline (existing)
  → Copy Line = that one row, same line format
```

磁盘文件是会话档案。环只服务 UI 和「进程还活着时的短读」。两者都从同一次 `Emit` 写出，不从文件再灌回环。

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `LogSystem` | 创建/持有会话文件 sink；刷盘；暴露绝对路径；Shutdown flush | 项目目录、剪贴板、JSON |
| File sink（spdlog，经现有 `SpdlogTextSink` 或同等薄包装） | 把 `LogRecord` 写成一行并按级别 flush | 过滤、UI |
| `LogConsoleStorage` | 环、generation、是否已截断 | 磁盘 |
| `ProjectManager::OpenProject` | 在项目根下写指针文件 | 打开第二个日志流 |
| `EditorAutomationSurface` | `editor.get_log` 的校验与 JSON | 读文件内容 |
| `ConsoleWindow` | Copy Visible、Copy Line、Ctrl+C | 会话文件格式 |

## Boundaries

- Allowed：`ProjectManager` 在打开成功后调用 `LogSystem` 的「发布指针」；Editor 读路径与环；MCP 只经 `editor_query`。
- Forbidden：Editor 或 MCP 再挂一套独立 logger；`get_log` 去解析 `.log`；Core 依赖 ImGui / Automation。

## Dependencies

- Depends on：spdlog 文件 sink（已在第三方树中）；`ProjectManager::OpenProject`；Automation capability/handler 配对。
- Depended on by：minCraft / MCP 客户端（只消费 op 与文件路径）。
- New dependency direction justified because：项目指针是「打开项目之后才知道根目录」的一次性副作用，不把 `LogSystem` 变成项目管理器。

## Ownership & Lifetime

- 会话文件路径与 spdlog logger 由 `LogSystem` 进程级状态持有，`Initialize` 创建，`Shutdown` 里 `Flush` 后释放 sink 并 `spdlog::shutdown()`。
- 指针文件是普通文件，无运行时对象。项目关掉后指针可以指向已经不再写入的路径；下次 `OpenProject` 覆盖它。
- 环仍是 `LogConsoleStorage` 的静态缓冲。`Clear`（Console 的 Clear）只清环，**不清**磁盘文件。

## Data Flow

`ME_LOG` → `LogRecord` → 三个 sink 并行：stdout、会话文件、环。  
`OpenProject` → 读当前文件路径 → 写 `latest-session.txt`。  
`editor.get_log` → 读环尾 + 两个路径 → JSON。  
Copy Visible / Copy Line → 格式化字符串 → 剪贴板。不读文件。

## Control Flow

- 写日志：调用线程同步 `Write`（与今天一致）。周期 flush 若使用后台线程，只调用 logger `flush`，不调用 `Emit`。
- 指针：仅 `OpenProject` 成功路径调用一次。
- Query：Automation host 线程（与其它 query 相同）同步读环的锁并返回。不异步。

## State & Invariants

- 未 `Initialize`：无会话文件，路径为空。
- 已 `Initialize` 且文件打开成功：路径非空，直到 `Shutdown`。
- 文件打开失败：引擎继续跑；路径保持空；stdout / 环仍工作。不重试创建，避免每条日志都碰磁盘失败。
- `ringTruncated` 在环开始覆盖旧记录时为真。`Clear` 清回假。磁盘不受 Clear 影响。
- 非法：两个会话文件同时被本进程追加。指针文件永远不作为 sink。

## API / Interface Semantics

### 会话文件

- Responsibility：打开本进程唯一日志文件并接收 `Emit`。
- Inputs / outputs：无业务参数。只读访问返回绝对路径 `std::filesystem::path`（空 = 未打开）。
- Errors：创建目录或打开文件失败 → 记到 stdout（此时 stdout sink 已挂上），继续无文件。
- Sync：`Initialize` 内同步打开。
- Thread-safety：与现有 `Emit` 相同，靠 `LogSystem` 的 mutex 串行 `Write`。

### 测试用目录

- 在 `Initialize` **之前**可指定会话目录（测试把日志放进临时目录）。未指定则用 `<exe>/Logs`。`Initialize` 已完成后再次指定无效。
- 生产启动路径不调用该入口。

### `editor.get_log`

- Responsibility：返回环的尾部和文件位置。
- Inputs：`limit`、`minSeverity`（见 Requirements）。
- Outputs：上文 Result。无匹配行时 `entries` 为 `[]`，`ok` 仍为 true。
- Errors：`minSeverity` 非法或 `limit` 类型不对 → 现有 Automation 错误对象（`InvalidPayload`），`ok` false。
- Sync / thread-safety：同步；读环时持有 `LogConsoleStorage` 的 mutex（通过现有 `CopyInto` / `Snapshot`，不新开无锁读）。

### 剪贴板

- Copy Visible 与 Copy Line 使用同一行格式函数，避免两套字符串。
- 行数很大时仍一次 `SetClipboardText`（过滤结果最多 2000 行量级）。不做分块。

## Failure Model

| 情况 | 行为 |
|------|------|
| `Logs/` 创建失败或文件打不开 | 无文件 sink；引擎启动继续；`logFile` 为 `""` |
| 指针文件写失败 | `ME_LOG` Warn；会话文件照常写；`sessionPointer` 为 `""` |
| 删旧日志失败 | 忽略该文件，继续启动 |
| `get_log` 时环为空 | `entries: []`，路径仍返回 |
| 进程崩溃 | 已 flush 的 Warn 和最近 ≤1 秒的 Info 应在文件中；未 flush 的最后一瞬 Info 可以丢失 |
| 搜索框聚焦时 Ctrl+C | 不拦截，交给输入框 |

## Alternatives & Trade-offs

### Option A（推荐）：exe 旁会话文件 + 项目指针 + query 只读环

- Summary：如上。
- Pros：`Initialize` 的第一行就能落盘，不依赖项目；minCraft 用指针或 `logFile` 找文件；MCP 响应用量有上限。
- Cons：未打开项目时要到 exe 的 `Logs/` 找；共享引擎安装时不同项目的会话文件在同一目录（指针把「当前这次」指回项目）。

### Option B：只写 `<ProjectRoot>/Saved/Logs`

- Why not selected：`Initialize` 早于 `OpenProject`，启动日志要么丢、要么先缓冲再搬家。无项目的 Game/工具同样没地方写。

### Option C：MCP 返回文件全文

- Why not selected：材质风暴的日志可以很大；环外的历史用路径读文件即可。query 保持小。

## Integration Impact

- `LogSystem::Initialize` / `Shutdown`：多一个 sink。已有 `logging-channels` 测例仍应通过（它们看的是环和额外 sink）。
- `ProjectManager::OpenProject`：成功路径多一次写文件。
- Editor Automation：一个 query。TS MCP server **不改工具表**。
- `ConsoleWindow`：按钮文案、右键、快捷键。过滤逻辑不动。
- Clear on Play / Clear：仍只清环。

## Migration / Compatibility

- Compatibility promises：不承诺旧版本有日志文件。新进程开始写新文件。
- Migration / rollout：无数据迁移。minCraft 在引擎合入后改为读 `latest-session.txt` 或 `editor.get_log` 的 `logFile`，可以停掉「自己重定向 stdio」的临时办法。
- Transitional code：无双写 logger。stdio sink 保留。

## Verification Strategy

- **S01：** 临时目录测例。测试进程里 `TestContext` 会先 `Initialize`，测例须 `Shutdown` → 指定目录 → `Initialize`，结束时再 `Shutdown`、清掉目录覆盖、`Initialize` 恢复默认，避免后面的 suite 写进临时目录。打一条 Info、一条 Warn，**不**先调用整系统 `Flush`。断言文件存在且 Warn 行已在文件中（Warn 的 flush 可以把此前还在缓冲区里的 Info 一起写下，不要求 Info 当时必须不在）。断言目录里已有 20 个更旧的 `minEngine-*.log` 时，打开新会话后仍只留 20 个，且最旧的那个文件名消失。直接调用发布指针：`latest-session-<stem>.txt` 的那一行等于会话路径。
- **失败路径：** 将目录指到不可写位置（若平台允许），断言 `Initialize` 不抛、路径为空、环仍收到记录。
- **S02：** 构造若干 `LogRecord` 进环，经与 handler 相同的纯函数得到 JSON：`limit` 钳制、`minSeverity` 过滤、`ringTruncated`、非法 severity 拒绝。Host 登记：`editor.get_log` 出现在 capabilities 且 handler 覆盖检查通过（现有 assert）。
- **S03：** 手工。Copy Visible 在开了 Channel 过滤时粘贴行数等于界面 `Visible` 计数，行与行之间有换行。右键一行只得到该行。焦点在搜索框时 Ctrl+C 复制的是搜索文字。
- **回归：** `test logging-channels`。stdio 指到 NUL 的手工或脚本：启动后会话文件里仍有 `LogSystem Initialized`。

## Open Questions

- 周期 flush 用 spdlog 后台线程还是每帧 `Flush` — **accepted**：spdlog `flush_every(1s)` + 文件 logger `flush_on(warn)`。`flush_every` 会连 stdout logger 一起周期 flush，可以接受。`Shutdown` 走 `spdlog::shutdown()`，停掉该线程。
- 非 Windows 的 exe 目录 — **accepted**：没有进程 exe 路径时跳过文件 sink，引擎继续启动。

## 变更记录

- 2026-10-19：S01–S03 已落地。`test logging-channels` 6/6 PASSED。Console 剪贴板仍待手工点验；无单独 Implementation Plan（按 Design Must 切片直接实现）。
- 2026-10-19：开工前评审修订。指针文件带 exe stem，避免 Editor/Game 互盖；`ringTruncated` 改为「上次 Clear 之后」；时间戳格式钉死；文件名加毫秒；写明测试里 Shutdown/恢复 `Initialize` 的顺序。

## Design Review

- **Verdict:** `Ready with deferred items`
- **Reviewer / date:** 2026-10-19（开工前评审，修订后实现）
- **Link or summary:** 阻塞项已收进上文（指针文件名、截断语义、时间戳、测试生命周期）。延后项：非 Windows 文件 sink；S03 剪贴板为手工验收。

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved or N/A
- [x] Major state/invariants resolved or N/A
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered where meaningful
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred/accepted

**Ready for implementation?** `yes` — 评审阻塞项已写入本文件。非 Windows 与剪贴板手工项不挡编码。

## Acceptance Checklist

- [x] Design review complete when required by complexity/profile
- [x] Implementation plan traces to design decisions (L2+) — waived: slices lived in Design Must；无独立 Impl 文档
- [x] Progress log updated when status changes
- [x] Feature registry status synced
- [x] `minEngineTests.exe test logging-channels` PASSED（含会话文件 / 裁剪 / 不可写目录 / Tail）
- [ ] Console：Copy Visible / Copy Line / Ctrl+C / Show Log File 手工点验
