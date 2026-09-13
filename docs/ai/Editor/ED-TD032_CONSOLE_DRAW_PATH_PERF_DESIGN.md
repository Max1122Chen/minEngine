# ED-TD032 — Console Output Draw-Path Perf

## Meta
- **ID:** `TD-032`
- **Type:** Design Spec（Tech Debt pay-down）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-13
- **Related:**
  - [TECH_DEBT.md](../TECH_DEBT.md) — TD-032 行
  - [CORE-F17 Logging Channels](../Platform/Core/CORE-F17_LOGGING_CHANNELS_DESIGN.md) — `LogRecord` / `LogConsoleStorage`
  - [ED-F09 Log Console UI](./ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md) — S03 与本债对齐
  - [BUG-EDITOR-003](../bugs/BUG-EDITOR-003.md) — **旁路发现**；本债 **不是** 最大化 Viewport 卡顿主因（已 Verified：`MaximumEditor.exe`）
- **Code today:**
  - `minEngine/.../Runtime/Core/Log/LogConsole.*`
  - `minEngine/.../Runtime/Core/Log/LogRecord.h`
  - `Editor/.../UI/EditorWindows/ConsoleWindow.*`

## TL;DR

CORE-F17 把 Console 改成存 `LogRecord` 后，**写路径变轻、读路径变重**：时间戳从 Push 挪到每帧 Draw；`Snapshot()` 每帧全表深拷；过滤又留了一堆「永远可见」的 Channel。结果是 **Output 打开 + ring 接近满** 时，主线程每帧付出可预期的 O(可见行) 系统调用式格式化 + O(容量) 字符串拷贝。  
本文件定清 **现状 → 问题机制 → 推荐解法与切片**；实现可先于 ED-F09 UX 落地，也可与 stash 中已有 `generation` / `CopyInto` 合并。

---

## Scope

### In
- 分析并修复 **LogConsoleStorage 写/读 API** 与 **ConsoleWindow Output Draw** 的热路径浪费
- 给出可验收的切片顺序与「Done」标准
- 明确与 ED-F09（Channel 多选等 UX）的边界：本债修 **成本**，ED-F09 修 **产品过滤面**

### Out
- 虚拟列表 / ImGui clipper（可选后续；本债不强制）
- 跨进程 / 文件日志索引、异步 UI 线程
- 改 spdlog Sink 格式（文本文件/stdout）
- BUG-EDITOR-003 进程名 / GPU 耦合（已另案关闭）

---

## Reader quick start

1. 本文件 **§3.0 大白话**（先读）→ §4 方案  
2. §5 切片与 §7 验收  
3. 实现时再对 §3.1 / 代码入口

---

## 1) 背景

### 1.1 怎么发现的

排查 BUG-EDITOR-003（最大化后 Viewport 导航卡顿）时，对比了旧 spdlog `LogConsoleEntry` 与 F17 后的 `LogRecord` 路径。用户确认：**卡顿主因是 `Maximum.exe` 进程名与主机 GPU/OS 策略耦合**，不是 Logging。  
但对比过程里，Console draw 路径的退步被钉成 **TD-032**：即使不导致全屏卡顿，也是 Editor 打开 Output 时实打实的主线程税。

### 1.2 设计哲学对齐

- **Mechanism over Policy：** Storage 提供廉价快照/世代；过滤策略留在 Editor View。  
- **Prefer Simplicity：** 先修「每帧重复做 Push 时就能做的事」和「无意义的全表拷」，不引入第二套日志模型或异步日志服务。  
- **Agent-Friendly：** 日后 Agent 查询若复用同一 Storage，同样受益于 generation / 低拷贝 API。

---

## 2) 现状（代码真相）

### 2.1 数据流（今日）

```text
ME_LOG / Emit
    │
    ▼
LogSystem → sinks[]
    ├─ SpdlogTextSink   （文件/控制台文本，本债不改）
    └─ LogConsoleSink::Write(const LogRecord&)
            │
            ▼
        LogConsoleStorage::Push(LogRecord by value)   ← 再拷一次进 Push 形参
            │  mutex
            │  满则 erase(begin) 单条  ← O(n) 搬移
            │  emplace_back(move)
            ▼
        s_Entries  (vector, cap ≈ 2000)

每帧 ConsoleWindow::DrawOutputTab（Output Tab 可见时）
    │
    ├─ Snapshot() → return s_Entries  ← 整表拷贝 vector + 每条 LogRecord 的 string
    ├─ （Pause 时再拷一份到 m_PausedEntries）
    ├─ for each record:
    │     PassFilter  → Core/App 可关；**其它 Channel 永远通过**
    │     FormatTimestamp → to_time_t + localtime_s + snprintf + std::string
    │     ImGui Selectable + TextColored
    └─ Copy 时再拼一遍带时间戳的字符串
```

### 2.2 `LogRecord` 体积（为何「拷贝」贵）

```cpp
struct LogRecord {
    system_clock::time_point timestamp;
    LogSeverity severity;
    const LogChannelBase* channel;
    std::string channelName;   // 常冗余（有 channel 指针时）
    std::string message;       // 主成本
    LogSourceLocation source;
    uint32_t threadId;
    std::vector<LogField> fields;  // 目前占位，仍有小向量开销
};
```

满缓冲时 `Snapshot()` ≈ **最多 2000 次** `LogRecord` 深拷（至少 `message` / `channelName` 各一次分配或 SSO 拷贝）。

### 2.3 UI 过滤（S00 最小适配遗留）

`PassFilter` 今日只对名为 `"Core"` / `"App"` 的 Channel 尊重勾选；**其它已注册 Channel 一律显示**。  
Severity / Search 仍有效，但「关掉 Core/App」并不能显著减少可见行——反而更容易在 Asset / Render / Physics 等通道刷屏时把 ImGui 行数顶满。

### 2.4 与旧 Entry 路径的差异（为何感觉「变差了」）

| 点 | 旧 spdlog Entry（印象） | 今日 LogRecord 路径 |
|----|-------------------------|---------------------|
| 时间戳 | 往往在入队时已格式化进字符串 | **仅存 `time_point`，Draw 每行重算** |
| 快照 | 同类问题可能已存在 | **明确每帧 `Snapshot()` 返回新 vector** |
| 过滤 | Core/Client 二分与模型一致 | 模型已多 Channel，UI 仍 Core/App |

F17 正确地把「真源」统一到 `LogRecord`；欠账是 **没有把「展示用派生数据」放回写路径或缓存层**。

---

## 3) 潜在问题

> **先读 §3.0 大白话。** §3.1 起是给实现用的短机制摘要，不必先啃。

### 3.0 大白话：每个问题到底在说什么

先建立一张心智图（不用记名字）：

1. **引擎里某处打了一条日志**（比如资源导入失败）。  
2. 这条日志会被**抄一份**放进一个叫「控制台仓库」的公共篮子里（最多大约 2000 条；满了就丢掉最老的）。  
3. 编辑器每一帧要**重画**界面。只要你开着 Console 的 **Output** 页，它就会：  
   - 去仓库里要一份「当前所有日志的拷贝」；  
   - 按过滤规则挑出要显示的行；  
   - 给每一行算出屏幕上的文字（含时间、通道名、级别、正文）；  
   - 交给 ImGui 画出来。

问题不在「有日志系统」，而在：**很多本可以做一次的事，被放进了「每一帧、每一行」里反复做**；再加上过滤没跟上多通道模型，屏幕上往往画得比你以为的还多。

下面按编号讲。**严重度高的先讲。**

---

#### P1 — 同一条日志的时间，每一帧都重新「翻译」一遍（高）

**你在屏幕上看到的：**  
`[14:32:07] [Asset] [Warn] missing meta …`  
前面那个 `14:32:07` 看起来只是普通文字。

**仓库里实际存的：**  
不是这串文字，而是一个「原始时刻」（机器时钟上的一个时间点）。  
要变成 `14:32:07`，程序必须：换算成本地时区 → 拆出时/分/秒 → 再拼成字符串。

**糟糕之处：**  
这条日志只要还在仓库里、还显示在列表上，**编辑器每一帧（一秒可能 60～120 次）都会对这一行重新做一遍「翻译」**。  
一条日志从进仓库到被挤出，可能活几十秒；若列表有一千多行可见，就等于每秒做几万～十几万次「翻译」。  
而这条日志的钟点在显示粒度上（精确到秒）几乎不变——**算出来的结果每次都一样，纯属白干。**

**什么时候特别疼：**  
Output 开着、几乎不过滤、仓库快满、日志又多。你就算停手不操作，只要界面还在刷，税还在交。

**直观类比：**  
书店书架上每本书本来就印好了出版日期；你却规定：每次客人走进店，店员必须把架上每一本书的日期从「出版社原始档案」重新誊一遍贴到书脊上——而日期根本没变。

**修的方向（白话）：**  
日志**刚写进仓库时**就把 `14:32:07` 算好存旁边；画画时直接读现成的字。别每帧现算。

---

#### P2 — 没新日志，也每帧把整篮日志「复印」一遍（高）

**仓库是大家共用的。**  
打日志的线程可能在写；画界面的线程要读。为了别读到一半被改乱，常见做法是：**给画界面的人复印一整份**，让他拿复印件慢慢画。

**今日的做法过头了：**  
只要 Output 在画，**每一帧都整篮复印**——哪怕上一帧到这一帧之间**一条新日志都没有**。  
篮子最多约 2000 条，每条还带着一整段消息正文（可能很长）。复印 = 把这些文字内容再复制一份到内存里。帧率越高，白复印次数越多。

**Pause（暂停）** 时再留一份「定格复印」是合理的（你要盯着那一刻的内容）。  
不合理的是：**实时模式、内容完全没变，仍每帧全量复印。**

**直观类比：**  
监控室墙上的告警板其实没变；保安却规定：每秒钟把整块板子重新抄到一张新草稿纸上再读——而不是看一眼「版本号没变就继续看旧草稿」。

**修的方向（白话）：**  
给仓库加一个「版本号 / 世代」：有人写入或清空就 +1。画界面时先问版本号；**没变就继续用上一帧的复印件**；变了再复印。

---

#### P3 — 篮子满了以后，每来一条新日志就「抽掉最上面一张再整叠下移」（中）

仓库容量有上限（约 2000）。满了还要收新日志，就得丢掉最老的。

**今日做法：**  
每来一条新的，就只扔掉最老的那一条。  
实现上像一叠纸：抽出最底下那张，上面将近 2000 张都要往下挪一格。  
日志刷得越猛（导入、编译、报错风暴），这种「整叠挪动」就越频繁。

**为什么你可能感觉到卡一下：**  
挪动的时候会短暂锁住仓库（不让别人同时读写）。写日志和画界面抢同一把锁时，界面那一侧也得等。

**直观类比：**  
电梯限乘 2000 人的名单是一张长表。每上来 1 个新人，就划掉表头 1 人，然后把后面 1999 个名字全部往前誊一格——而不是隔一段时间划掉一批、或用环形座位「覆盖最老的位子」。

**修的方向（白话）：**  
满了就一次丢掉一批（比如 200 条），摊薄「整叠挪动」的次数。略牺牲「严格一条条 FIFO」的观感，换写路径平稳。

---

#### P4 — 一条日志进仓库前，被「抄两遍作业」（中低）

打日志时，系统已经组好了一份完整记录（时间、级别、通道、正文……）。  
要放进 Console 仓库，本来**抄进篮子一次**就够了。

**今日多了一道手续：**  
先把这份记录再抄成「递给仓库函数的临时副本」，函数里再挪/拷进篮子。  
等于进门前在玄关又复印了一份，进门后再放书架——玄关那份随即扔掉。

**体感：**  
单条不致命；日志很密时，等于写路径无故多做一次「整份记录」的复制（正文长时更明显）。  
比 P1/P2 轻，但是**干净的小修补**，顺手该做。

**直观类比：**  
快递已经装好箱；仓库规定：先在门口拆开再原样封一箱交给理货员，理货员再拆开上架——门口那箱纯属多余。

---

#### P5 — 过滤开关跟「多通道」对不上，列表往往比你想的更挤（中，会放大 P1）

新日志系统里，日志可以来自很多**通道**（Core、App、Asset、Render……），不再只有「引擎 / 游戏」二分。

**今日 Output 的过滤却还像半成品：**  
勾选框基本上只认真对待 **Core** 和 **App**。  
其它通道的日志——**关不掉，默认一直显示**。

于是：你以为「我关掉 Core 能清净一点」，但 Asset / Render 等仍在刷屏；屏幕上要画的行数仍然很多。  
行数一多，P1（每行每帧翻译时间）和 ImGui 画行成本就被**放大**。

这既是**产品体验问题**（ED-F09 要做完整通道多选），也是**性能放大器**（所以 TD-032 也要管）：  
就算时间戳和复印都修好了，若永远画出「其它通道」两千行，界面照样沉。

**直观类比：**  
电视台有几十个频道，遥控器却只有「中央台 / 地方台」两个键；其它台永远强制播放。你关了中央台，客厅里还是很吵。

**修的方向（白话）：**  
至少能关掉「其它通道」，或提供真正的通道多选（ED-F09）。先减要画的行，再谈更花哨的优化。

---

#### P6 — 还知道、但不急着本期啃的事（低 / 后续）

这些不是「假问题」，而是**下一层**；本债先把 P1–P5 按住即可。

| 白话 | 含义 |
|------|------|
| **列表没有「只画看得见的那几行」** | 即便过滤后仍有很多行，ImGui 可能对大量行都做控件逻辑。业界常用「虚拟列表」：只精心绘制当前滚动窗口里那二三十行。那是后续增强，不是 TD-032 门槛。 |
| **一条记录里名字可能存了两份** | 既有「指向通道对象的指针」，又可能再存一份通道名字符串。略费内存，不是当前卡顿主因。 |
| **以后做 Collapse / 时间窗要小心** | ED-F09 若每帧把两千条重新分桶聚合，可能**新造**一个类似 P1/P2 的坑。功能可以做，但要守同一纪律：别在每帧热路径里重复造轮子。 |
| **读写抢同一把锁** | 修好「少复印、少挪桌子」之后，锁住的时间会短很多。上无锁环形缓冲之类，对现在的规模通常过重。 |

---

#### 明确不是本债的锅

| 现象 | 怎么理解 |
|------|----------|
| **最大化窗口后 Viewport 导航卡顿（BUG-EDITOR-003）** | 已证实主要是进程名 `Maximum.exe` 和机器上 GPU/系统策略耦合；改成 `MaximumEditor.exe` 后已验证。**不是**上面这些日志问题导致的。本债是排查时顺手发现的「Output 打开时的主线程税」。 |
| **写到文件 / 终端的那份 spdlog 文本** | 另一条管道；不修它也能修好 Console 窗口。 |
| **结构化字段 LogField 还是空壳** | 现在几乎不占事，别往那枪口上对。 |

---

### 3.1 机制摘要（实现对照）

#### P1 — Draw 侧每行 `FormatTimestamp`（高）

**机制：** 每个通过过滤的可见行，每帧：`to_time_t` → `localtime_*` → `snprintf` → 临时 `std::string`。  
**放大：** Output 开着、过滤松、ring 近满。  
**性质：** 同一秒级显示结果被重复计算。

#### P2 — 每帧全表 `Snapshot()` 深拷（高）

**机制：** 每帧返回整份 `vector<LogRecord>` 拷贝；无新日志也拷。  
**放大：** 长 message、满缓冲、高帧率。

#### P3 — 满缓冲时 `erase(begin)` 单条丢弃（中，写路径）

**机制：** 每 Push 删首元 → 近满时整段前移；拉长持锁，拖累读写。

#### P4 — Sink → Push 多一次拷（中低，写路径）

**机制：** `Write(const&)` 再 `Push(by value)`，入队前多一次完整 Record 拷贝。

#### P5 — Filter 与多 Channel 错位（中，放大 P1）

**机制：** 仅 Core/App 可关；其它 Channel 永远可见 → 可见行偏多。

#### P6 — 次要 / 后续

ImGui 无 clipper；`channelName` 冗余；Collapse/时间窗的每帧成本；锁粒度优化过度设计。

#### 非问题

BUG-EDITOR-003；spdlog 文本 Sink；空的 `LogField`。

---

## 4) 解决方案（拍板 · 2026-09-13）

### 4.1 原则

1. **完整时刻 + 本地显示串并存**：`timestamp` 保留真源；`displayTime` 入队前译一次。  
2. **UI 热路径禁止无条件全表深拷**：generation 未变则复用缓存。  
3. **满容量用环形缓冲覆盖最老槽**，不做 `erase(begin)` / 也不靠 batch-drop 凑合。  
4. **入队只允许一次 Record 拷贝**：`Push(const LogRecord&)`，去掉「形参再拷」。  
5. **Channel 过滤（原 P5）：本期不做**（维护者判定非问题）。  
6. **虚拟列表只减「画行」成本，不缩小 Snapshot**：clipper 与 generation/CopyInto 是两层；本期可做 clipper，索取仍按 generation 全量（有序）拷贝。

### 4.2 拍板条目

| # | 问题 | 策略 | 本期 |
|---|------|------|------|
| 1 | P1 每帧翻译时间 | `LogRecord` 保留 `timestamp`，并增加 `displayTime`（如 `HH:MM:SS`）；在 **Emit 或入 Console 仓前** 填一次；Draw 只读字符串 | **做** |
| 2 | P2 静止全表拷 | `GetGeneration` + `CopyInto`；ConsoleWindow `m_LiveCache` | **做** |
| 3 | P3 删头搬移 | **环形队列**（固定容量，覆盖最老）；`CopyInto`/`Snapshot` 按从旧到新展开 | **做** |
| 4 | P4 双拷 | `Push(const LogRecord&)`；Sink 直接传入引用 | **做** |
| 5 | P5 通道过滤 | — | **不做** |
| 6 | P6 只画可见行 | `ImGuiListClipper`（可先收集通过过滤的下标再 clip）；**不**据此缩小 Snapshot 索取大小 | **做**（draw） |

### 4.3 API / 数据契约（目标）

```cpp
struct LogRecord {
    std::chrono::system_clock::time_point timestamp{};
    std::string displayTime; // local "HH:MM:SS", filled once before console store / at Emit
    // ... severity, channel, message, ...
};

class LogConsoleStorage {
public:
    static void Push(const LogRecord& record); // one copy into ring slot; EnsureDisplayTime if empty
    static void CopyInto(std::vector<LogRecord>& out); // chronological oldest→newest
    static uint64_t GetGeneration();
    static std::vector<LogRecord> Snapshot(); // cold path / tests
    static void Clear();
};
```

**环形语义：**

- 容量 `kMaxEntries`（仍 2000）。  
- 未满：顺序追加。  
- 已满：覆盖最老槽，逻辑起点前移；**generation++**。  
- Clear：清空逻辑内容，generation++。

**Draw：**

```text
if (gen != cacheGen) CopyInto(liveCache); cacheGen = gen
可选 Pause 定格拷贝
建 visibleIndices（PassFilter）
ImGuiListClipper over visibleIndices.size()
行内用 entry.displayTime（空则极短兜底，避免再每帧 localtime 热路径）
```

### 4.4 明确不做 / 延后

| 项 | 结论 |
|----|------|
| 退回整行 `string` Entry | 拒绝（破坏 F17 真源） |
| 无锁 SPSC ring | Deferred（过重） |
| 用 clipper 缩小 Snapshot 窗口 | **不做** — 过滤/总数/AutoScroll 仍需全量逻辑视图 |
| Channel 过滤 E0/E1 | 留给 ED-F09；本期跳过 |
| batch-drop 代替 ring | **废弃**（改用环形） |

### 4.5 目标数据流

```text
Emit
  ├─ 填 timestamp
  └─ 填 displayTime（一次 local 翻译）
        │
        ▼
ConsoleSink::Write(const&)
        │
        ▼
LogConsoleStorage::Push(const&)
  ├─ 入环一拷（满则覆盖最老）
  └─ ++generation

Draw:
  gen 未变 → 复用 m_LiveCache
  gen 变了 → CopyInto（整环展开为有序 vector，仅此时付拷贝）
  clipper 只画可见过滤行；读 displayTime
```

---

## 5) 实施切片

| Slice | 内容 | 验证 |
|-------|------|------|
| **L1** | `displayTime` + Emit/Push 填充；Draw 停用每帧 `localtime` | Output 开着刷日志更稳 |
| **L2** | generation + `CopyInto` + UI live cache | 无新日志时不白拷 |
| **L3** | 环形缓冲替换 `erase(begin)` | 持续刷屏无删头尖刺 |
| **L4** | `Push(const&)` 去掉双拷 | 代码审阅 |
| **L6** | Output 列表 `ImGuiListClipper` | 大列表时只建可见行控件 |
| ~~L5~~ | Channel 过滤 | **取消（本期）** |

**节奏：** 当前 `feat/editor` 一次落地 L1–L4+L6 → 勾验收 → 再 stash 恢复 ED-F09（合并时保留 ring/generation/displayTime）。

**stash 注意：** ED-F09 的 batch-drop **以本文件环形方案为准覆盖**；其 generation/`CopyInto` 可吸收。

---

## 6) 与 ED-F09 stash 的关系

`stash@{0}`（ED-F09）已包含：

| 项 | stash 状态 | 本拍板 |
|----|------------|--------|
| generation / CopyInto / live cache | 有 | **保留** |
| kDropBatch | 有 | **改为环形，删 batch-drop** |
| Channel 多选等 UX | 有 | 产品面；与 P5「本期不做」不冲突（恢复 F09 时再上） |
| Push 侧时间戳 | **无** | **本债补** |
| Sink 双拷 | **未见** | **本债补** |
| ImGuiListClipper | **未见** | **本债补** |

---

## 7) 验收标准

- [x] Output 打开、缓冲近满、无新日志：不每帧全量深拷（generation 短路）  
- [x] 展示时间戳：不每行每帧 `localtime_*`（读 `displayTime`）  
- [x] 满缓冲持续 Push：环形覆盖，无单条 `erase(begin)` 搬移  
- [x] Console 入队无「形参 + 容器」双份无意义拷贝  
- [x] Output 大列表：clipper 只绘制可见行（Copy/计数仍可扫过滤集）  
- [x] ~~非 Core/App 可关~~ **本期不作验收**（维护者决定）  
- [x] `logging-channels` 等 Log 测试绿（含 ring/displayTime 用例）；Clear / Pause / Copy / AutoScroll 代码路径保留  
- [x] TECH_DEBT TD-032 → **Done**；Progress 一笔；ED-F09 S03 回链本文件

**非验收：** BUG-EDITOR-003 Viewport 帧时间。

---

## 8) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| `displayTime` 增大每条体积 | 内存略增 | 仅短串 `HH:MM:SS` |
| ED-F09 DateTime 模式 | 串格式不够 | F09 恢复时模式切换一次性重填或加第二字段；勿退回每帧 localtime |
| 环形 + CopyInto 展开 | 有日志时仍全量拷 | 正确代价；静止时 generation 免拷 |
| clipper + 过滤下标 | 每帧建 indices | O(n) 下标远轻于千行 Selectable |
| 与 ED-F09 stash 冲突 | 合并痛 | Storage 以本文件为准（ring > batch-drop） |
| 误当 BUG-003 fix | 范围乱 | Meta 已标明旁路 |

---

## 9) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done** |
| Blocked on | 无 |
| Next action | stash 恢复 ED-F09/F11；准备 commit |
| Owner | project maintainer |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-13 | 初稿：现状 / P1–P6 / 推荐解法 / 与 ED-F09 stash 对照 |
| 2026-09-13 | 增补 §3.0 大白话导读（不依赖读代码） |
| 2026-09-13 | **拍板：** displayTime + generation + **环形** + Push(const&)；P5 不做；clipper 做但不缩 Snapshot；开始实现 |
| 2026-09-13 | **Done：** 实现落地；`logging-channels` 19/19 |
