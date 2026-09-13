# ED-F09 — Editor Log Console (LogRecord UI) — Design Spec

## Meta
- **ID:** `ED-F09`
- **Type:** Feature
- **Status:** Done（W0–W2 已落地；过滤预设持久化留给 ED-F10）
- **Owner:** project maintainer
- **Last updated:** 2026-09-13
- **Branch:** `feat/editor`
- **Depends on:** `CORE-F17` **Done**（`LogRecord` / `LogChannel` / `LogConsoleStorage`）
- **Related:**
  - [CORE-F17 Design](../Platform/Core/CORE-F17_LOGGING_CHANNELS_DESIGN.md)
  - [ED-F04 Command Console](./ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md)（同窗 Command Tab；本 Feature **不改** Command 语义）
  - [ED-F11](./ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md)（Console 为共享 Service 面板）
  - 后续：`ED-F10` 可持久化过滤预设
  - Code: `Editor/src/UI/EditorWindows/ConsoleWindow.*` · `Runtime/Core/Log/*`
- **Blocks:** 无

## TL;DR

把 Maximum **Console → Output** 建成完整的 **诊断过滤面**：以 `LogRecord` 为真源，支持 **多维度过滤**（Channel / Severity / 文本 / 时间窗 / 源位置等），并对齐业界常用维度；删除过时的 Core·App 二分。Command Tab 不动。实现按切片交付，但 **目标能力一次定清**，不当成「只做 Channel 勾选的临时 MVP」。

---

## Pre-flight（2026-09-13）

| 项 | 结论 |
|----|------|
| 扫描 | 已读 `LogRecord`；过滤几乎只有 Core/App + Severity；`timestamp` / `source` / `threadId` 已在 Record 上，UI 未用尽 |
| 前置 | **sound**；需 `LogSystem` 枚举 Channel API |
| 债风险 | **low–medium** — Collapse / 时间窗要小心每帧成本 |
| WIP | 与 ED-F11 可交错 |
| Philosophy | 过滤是 View；不发明第二套日志模型；Agent 日后可复用同一过滤描述 |
| 建议 | **Go** — 目标架构完整；切片分波交付 |

---

## Scope

### In（产品目标，分波实现）

**过滤 / 展示维度（见 §3）**

- Channel 多选（注册表枚举）
- Severity 多选（或等价）
- 文本 Search（message；可选含 channel 名）
- **时间：** 显示模式 + **时间窗过滤**（见业界对比与推荐）
- 源位置：`file:line` 展示 / tooltip；可选按文件名片段过滤
- Collapse（相同消息聚合）
- Clear / Copy / AutoScroll / Pause；Clear on Play（对齐 Unity/UE 习惯）

**基础设施**

- 删除 Core/App Source UI
- `LogSystem` 只读枚举已注册 Channel
- 过滤状态进程内记忆；持久化留给 `ED-F10`

### Out（仍后置，但不否定目标）
- 点击跳转外部 IDE（可另 Feature）
- `LogField` 列展开（等 L1 字段落地后加维度）
- 在 Output UI 里直接 `SetChannelSeverity`（用命令 / Settings 另做）
- 改 ED-F04 Command 语义
- 完整正则引擎（Search 先 substring；regex 可二期）

## Reader quick start

1. §3.2 业界维度对比 + 本引擎维度表（必读）
2. §3.3 过滤模型
3. §8 交付波次
4. 现码：`ConsoleWindow::PassFilter`

---

## 1) 背景与目标

### Pain
- Channel 丰富后仍用 Core/App 二分，无法做专业诊断。
- `LogRecord` 已有时间、源位置、线程，UI 浪费了。

### 成功长什么样
- 能组合：「最近 10s + Asset/Render + Warn+ + 含 failed」一类查询。
- 刷屏可用 Collapse / 关 Channel / 时间窗压住。
- 过滤描述清晰，便于日后 Agent/`log.filter` 复用同一模型（本 Feature 先 GUI）。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| `LogRecord` | `timestamp`, `severity`, `channel`/`channelName`, `message`, `source`, `threadId`, `fields`（占位） |
| UI 过滤 | Severity 勾选 + Search；Channel 仅 Core/App 特判 |
| 时间 | 仅行内 `HH:MM:SS` 展示，**无时间过滤** |
| Collapse / Clear on Play | 无 |

---

## 3) 方案

### 3.1 总原则

1. **View 过滤 ≠ Emit 抑制** — UI allow-set 不替代 Runtime Channel Severity。
2. **过滤模型可描述** — 内存中有明确 `LogOutputFilter` 结构，GUI 只是编辑器。
3. **维度可扩展** — 新维度加字段 + Pass 函数，不推翻 Channel/Severity。
4. **Forward-only** — 删除 Core/App Source 段。

### 3.2 业界 Console 过滤维度（调研）

| 维度 | Unreal Output Log | Unity Console | 说明 |
|------|-------------------|---------------|------|
| **Category / Channel** | ✅ 主过滤 | ❌ 弱（无 UE 式 Category UI） | minEngine 对齐 UE |
| **Verbosity / Severity** | ✅ | ✅ Message/Warning/Error | 已有基础 |
| **文本 Search** | ✅（可正则） | ✅ | 已有 substring |
| **Timestamp 显示** | ✅ Timestamp Mode | ✅ Show Timestamp | 显示 ≠ 过滤 |
| **时间范围过滤** | ❌ 少见作一等 UI | ❌ | 调试「只看刚才」时很有用；Chrome/IDE 更常见 |
| **Collapse 重复** | 有限 | ✅ Collapse | 高频刷屏刚需 |
| **Clear on Play** | ✅ 常见 | ✅ Clear on Play | 与 Play Mode 对齐 |
| **Stack / 源位置** | 有 | ✅ Stack Trace 档位 | 我们有 `source`；完整栈后置 |
| **线程** | 偶见 | 少 | Record 已有 `threadId` |
| **自定义字段** | 分类为主 | — | 等 `LogField` |

**结论：** Channel + Severity + Search + Collapse + Clear on Play 是「编辑器标配」。  
**时间戳显示**是标配；**时间窗过滤**不是 UE/Unity 主打，但对帧卡顿/偶发错误排查很值，建议作为 **本引擎一等维度**（数据已在 Record 上，成本主要在 UI）。

### 3.3 本引擎过滤维度表（产品目标）

| 维度 | 优先级 | 行为 |
|------|--------|------|
| **Channel** | P0 | 多选 allow-set；默认全开；列表来自注册表 |
| **Severity** | P0 | 多选（保持现交互） |
| **Text** | P0 | message substring；可扩展「含 channel 名」 |
| **Timestamp display** | P0 | Off / Time / DateTime（对齐 UE Timestamp Mode） |
| **Time window** | P1 | 相对窗：`Last N seconds`；或 `Since Play Started`；或 `Since Clear`；可选绝对 From–To（后） |
| **Collapse** | P1 | **连续**相同 `(channel, severity, message)` 合并计数（非全局去重） |
| **Clear on Play** | P1 | 进入 Play 时 Clear Storage（可关） |
| **Source file** | P2 | tooltip 常开；可选 filename 含过滤 |
| **Thread** | P2 | 多选或「仅主线程」 |
| **LogField** | P3 | 等 L1 |

### 3.4 过滤模型（逻辑）

```text
struct LogOutputFilter
{
    // Channel: empty allow-set = show none; or explicit "all" flag
    bool allChannels = true;
    std::unordered_set<std::string> channelAllow; // by name; ptr match when possible

    bool severityEnabled[6]; // Trace..Fatal

    std::string textQuery; // case-insensitive substring

    enum class TimeMode { Off, LastSeconds, SincePlay, SinceClear, AbsoluteRange };
    TimeMode timeMode = Off;
    float lastSeconds = 10.f;
    // AbsoluteRange: optional later

    bool collapseDuplicates = false;
};

bool Pass(const LogRecord&, const LogOutputFilter&, const FilterContext&);
```

`FilterContext`：Play 是否中、Play 起始时间、上次 Clear 时间、当前时钟。

### 3.5 UI 草图

```text
[Clear ▾] [Copy] [AutoScroll] [Pause] [Collapse] [Clear on Play]
Filter: [Channels…] [Levels…] [Time: Last 10s ▾]  Search: [________]
─────────────────────────────────────────────────────────────
[12:01:02] [Warn] [Asset] (x14) Failed to load …     ← Collapse 计数
```

### 3.6 Runtime API

```cpp
static void ForEachRegisteredChannel(const std::function<void(LogChannelBase&)>&);
```

### 3.7 性能

- Collapse：单次 Snapshot 扫描建桶即可；注意 Pause 快照语义。
- Time window：用 `record.timestamp` 与 context 比较；避免每行分配。
- 若 Snapshot 拷贝成瓶颈 → 另开 TD（只读视图），不挡 P0。

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| 只做 Channel+Severity | **拒绝作为最终目标**；可作 S00–S01 交付波，但 Design 保留完整维度表 |
| 时间只用显示、不做窗 | 降级可接受，但 **推荐做 P1 时间窗** |
| 过滤写入 Runtime Severity | **不用**（与 Emit 策略耦死） |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 维度过多 UI 臃肿 | P0 进主条；P1/P2 进 Filter 下拉 |
| Collapse 与 Pause 交互含糊 | 文档：Collapse 作用于当前可见流；Pause 冻结输入集 |
| Since Play 依赖 Play Mode 时钟 | 从 `IEditorContext::IsPlaying` + 记录进入 Play 的时间点 |

---

## 6) 验收（产品目标；可按波次勾选）

**P0**
- [ ] 无 Core/App Source；Channel 多选来自注册表
- [ ] Severity + Search 可用
- [ ] Timestamp 显示模式可切换
- [ ] Editor 构建 + 双 Channel 手测

**P1**
- [ ] Time window（至少 Last N s **或** Since Play）
- [ ] Collapse 计数正确
- [ ] Clear on Play 可开关

**P2**
- [ ] Source tooltip；可选 file 过滤或 thread 过滤之一

---

## 7) Status note

（无）

---

## 8) 交付波次（完整目标，分期落地）

| Wave | 内容 |
|------|------|
| **W0** | 枚举 API；删 Core/App；Channel 多选 + Severity/Search 理顺 |
| **W1** | Timestamp 显示模式；Time window；Clear on Play |
| **W2** | Collapse；Source tooltip；Thread 或 file 过滤 |
| **W3** |（可选）Search 增强 / 过滤预设 → ED-F10 |

---

## 9) 开放点

| # | 问题 | 推荐默认 |
|---|------|----------|
| O1 | 时间窗先做哪一种 | **Last N seconds + Since Play**（Absolute 后置） |
| O2 | Collapse 键 | 仅与**上一可见行**比 `(channelName, severity, message)` |
| O3 | Clear on Play 默认开还是关 | **默认开**（Unity 感）；可关 |
| O4 | 过滤持久化 | **ED-F10**；本期进程内 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-11 | Planned 草稿 |
| 2026-09-13 | 充实为 Channel 过滤短刀 |
| 2026-09-13 | **修订：** 完整过滤维度表 + 业界对比；时间窗/Collapse/Clear on Play 纳入目标；改「仅 MVP」表述为分期交付 |
