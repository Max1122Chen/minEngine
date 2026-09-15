# ED-F11 — Multi-document Tab Host — Design Spec

## Meta
- **ID:** `ED-F11`
- **Type:** Feature
- **Status:** In Progress（W0–W2 Done；**W3 大部落地** — Pin/Close All/Reopen/路径/快捷键；Save As / Scene 多开策略仍可后置）
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/editor`
- **Depends on:** Editor Shell（`EditorSubModule` / `EditorServiceModule` / DockSpace）；`ED-F02` 打开资产路径
- **Related:**
  - [EDITOR_SHELL_DESIGN](./EDITOR_SHELL_DESIGN.md)
  - [ED-F02 Workflow](./ED-F02_EDITOR_WORKFLOW_DESIGN.md)
  - [ED-F09 Console](./ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md)
  - [EDITOR_COMMAND_HISTORY](./EDITOR_COMMAND_HISTORY.md)（Undo；本 Feature 要求 **per-Session 栈**）
  - [ENGINE_0_1_0_ROADMAP](../ENGINE_0_1_0_ROADMAP.md)
  - 后续：`ED-F10` Settings；World Query（建议 `ED-F12`）
- **Blocks:** MCP 富工具（软）；Query/Command 应打在 Session 上

## TL;DR

Maximum 采用 **可注册的文档类型 + 多 Session 文档宿主**（单 OS 窗口、共享 DockSpace **壳**）。  
同一类型可同时开多个 Session；**再次打开已打开的资产 → 聚焦已有 Session**。  
每个 Session **自有 CommandStack**。  
**方案 A ≠「所有子编辑器共用同一套面板」**——共享的是 Dock **宿主**；各文档类型注册自己的 **面板套件**（AnimClip 可有 Timeline/Track，Scene 没有）。

目标按完整产品能力设计；实现分波，不把「互斥单槽」或「写死 enum kind」当成最终形态。

---

## Pre-flight（2026-09-13 修订）

| 项 | 结论 |
|----|------|
| 扫描 | 互斥 SubModule + 每域单资产；Docking 已开；无文档 Host |
| 前置 | **sound** |
| 债风险 | **medium** — 注册式类型 + per-Session 栈 + 多实例域状态，是壳层真重构 |
| Philosophy | **Capabilities / 可组合**：文档类型可注册，Host 不写死业务 kind；**Agent-Friendly**：Session 一等公民；**Prefer Simplicity**：共享 Dock 壳，不做过早的每文档独立 DockSpace / 多 OS 窗 |
| 建议 | **Go** — 目标架构按本文；Wave 分期 |

---

## Scope

### In（产品目标）

**宿主与模型**
- `EditorDocumentHost`：Session 列表、Active、OpenOrFocus / Activate / Close / Reorder
- `EditorDocumentSession`：稳定 Id、**文档类型 Id（注册）**、资产身份、Dirty、域状态、**自有 CommandStack**
- **文档类型注册表**（非 C++ enum）：类型 Id、显示名、资产匹配、`CreateSession`、面板套件、策略标志
- 文档 Tab UI：排序、关闭、Dirty、**完整右键菜单契约（§3.4）**；按 Wave 分期实现，模型一次定清

**资产 ↔ Session 契约（硬）**
1. **允许**同时打开多个 **同类型** 的不同资产 Session（例：两个 Material、两个 AnimClip）。
2. **尝试打开已在某 Session 中打开的同一资产** → **只 Activate 该 Session**，不新建。
3. 资产身份键与 `AssetMeta` / 路径 / Guid 策略在注册与 Workflow 中统一（一资产一打开实例）。

**布局**
- **方案 A（澄清见 §3.1）：** 一个共享 `DockSpace` 壳 + **按文档类型（及 Active Session）切换面板套件可见性**；非 Active Session **保活状态**。
- 共享 Service 面板：Console、Content Browser、Main Menu 等 **类型无关**，常驻。

**Undo**
- **CommandStack 按 Session 分**（硬需求，非可选项）。

### Out（后置能力；不否定方向）
- 每 Session 独立全量 DockSpace 持久化（旧称方案 B）——可选二期增强
- `ViewportsEnable` 撕 OS 窗
- 多 Editor Group 分屏（VS Code 式）
- Prefab 隔离 RT 子编辑器（另 Feature）
- Query API 本体（`ED-F12`）

## Reader quick start

1. §3.1 方案 A 澄清（面板套件 ≠ 共享面板内容）
2. **§3.1.1 Mode → Active Session（多文档后的壳语义）**
3. §3.2 注册式文档类型
4. §3.3 资产打开契约
5. §3.4 文档 Tab 右键菜单契约
6. §3.5 per-Session CommandStack
7. §8 交付波次

---

## 1) 背景与目标

### Pain
- 互斥 SubModule：无法多会话对照编辑。
- Kind 写死 enum → 每加 AnimClip / Prefab Stage 都改 Host。
- 单栈 Undo 在多文档下会串台。
- **Mode 语义残留：** Window「编辑器模式」、整页 Reset Dock、面板绑死 Mode 与多 Tab 共存冲突。

### 成功长什么样
- 加新子编辑器 = **注册 DocumentType + 面板套件 + Opener**，Host / Tab 条零改或少改。
- 多开同类型不同资产；重复打开同资产则跳转。
- 切 Tab 换面板套件（可含 Track/Timeline）；后台 Session 不丢编辑与 Undo。
- **前台真相 = Active Session（+ Tab）**；不再用互斥 Mode 描述产品。
- Console / Content Browser 等 **Shared** 面板常驻，且在各类型默认布局中有槽。

---

## 2) 现状

| 项 | 今日 |
|----|------|
| Host | `EditorDocumentHost` + 注册表；OpenOrFocus / Tab（W0–W2） |
| SubModule | 仍作类型实现者；产品层勿再称互斥 Mode |
| 打开 | `TryOpenAsset` → `OpenOrFocus` |
| Undo | per-Session CommandStack（W2） |
| 布局债 | Material/AnimGraph 默认布局曾漏 dock CB；切类型整页 Rebuild 仍偏 Mode 时代 |

---

## 3) 方案

### 3.1 方案 A 是什么（重要澄清）

此前「方案 A / B」容易误解。定义如下：

| | **方案 A — 共享 Dock 壳 + 类型面板套件**（选用） | **方案 B — 每 Session 独立 DockSpace**（后置可选） |
|--|------------------------------------------------------|-----------------------------------------------------|
| Dock 宿主 | **一个** `DockSpaceOverViewport` | 每个文档 Tab 内嵌自己的 `DockSpace`（`KeepAliveOnly`） |
| 面板内容 | **每文档类型注册自己的窗口集合** | 同左，但布局状态 per-session 隔离更强 |
| 切文档 | 显示 Active 类型套件；隐藏其它类型套件；**Session 状态保活** | 切 Tab 即切整棵 dock 树 |
| 用户改 dock | 全局一份 ini（可再按「最后激活类型」记忆） | 每 Session / 每类型可独立 ini |
| 典型体验 | VS Code：**编辑器组共享工作台**；换文件换中间内容与相关视图 | UE 部分 Toolkit：**每个资产编辑器自己的布局** |

**方案 A 明确支持：**

```text
Active Session = AnimationClip
  → 显示：Track/Timeline、Clip Preview、相关 Inspector 页…
  → 隐藏：Scene Hierarchy/Viewport、Material Graph…（其它类型套件）

Active Session = Scene
  → 显示：Viewport、Hierarchy…
  → 隐藏：Track/Timeline…
```

**AnimClip 轨道编辑器** 不是「硬塞进 Scene 布局」，而是 **AnimClip DocumentType 的面板套件成员**。  
Host 只问：`type->GetPanelSuite()` / `ApplyVisibility(activeSession)`。

```text
┌─ Document Tab Bar ─────────────────────────────────────────┐
│ [Level.mescene] [Hero.memat] [Walk.meaclip*] [Idle.meaclip] │
└────────────────────────────────────────────────────────────┘
┌─ Shared DockSpace (one host) ──────────────────────────────┐
│  Suite(activeType) panels + Shared(Console/CB/…)           │
│  e.g. active=Walk.meaclip → Timeline + Track + Preview     │
└────────────────────────────────────────────────────────────┘
```

**为何先 A 不先 B：** 实现与调试成本低一个数量级；面板套件已能表达「有的编辑器有轨道、有的没有」。当出现「同类型两个 Session 要同时摆出两套完全不同的用户自定义 dock」再评估 B。

### 3.1.1 Mode → Active Session（多文档壳语义 · 2026-09-13）

单编辑器时代用 **Mode**（Scene / Material / AnimGraph 互斥）描述「当前在哪个编辑器」。多 Tab 后多种 Session **共存**，Mode 不再适用。

| 少说（遗留） | 多说（目标） |
|--------------|--------------|
| Mode / 进入 Material Mode | **Active Session** / 聚焦某文档类型的前台 Session |
| 互斥子编辑器 | **DocumentType 实现者**（`EditorSubModule` 可暂留作实现，产品语义降级） |
| Window → 切换 Mode | **Focus last session of type**（无则引导开资产）；**Tab 条为前台真相** |
| 整页 Reset Dock = 换 Mode | 切类型 **默认不拆整棵 Dock**；仅首次 / 用户 Reset；Shared 区宜稳定 |

**面板两层：**

```text
Shared（类型无关，常驻）
  Main Menu · Document TabBar · Content Browser · Console · …

Type Suite(T)（仅 ActiveSession.Type == T 时显示）
  Scene → Viewport, Hierarchy, …
  Material → Graph, Material Preview, …
  AnimGraph → Graph, Parameters, …
```

可见性：`Shared` = 空 `GetOwnerModuleId()`；`TypeSuite(T)` = owner 对齐该类型 ModuleId。  
**每种类型默认布局必须为 Shared 面板留 dock 槽**（至少 CB + Console），禁止「只在 Scene 布局里 dock CB」。

**激活契约：**

```text
OpenOrFocus / Activate(Session)
  → ActiveSession = S
  → 显示 TypeSuite(S.Type)，隐藏其它 TypeSuite
  → ActivateSession（恢复类型内槽状态）
  → 前台类型实现者（非「销毁其它编辑器」）
  → 必要时才 RequestResetLayout（类型首次或用户 Reset）
```

**本切片落地：** 设计落盘；CB 进入 Material/AnimGraph 默认布局；Window 菜单改为 Focus 语义；Shared 注释。整页「先壳后套件」布局与彻底去 Mode API 命名可后续 Wave。

### 3.2 注册式文档类型（取代 enum）

**拒绝：** `enum class EditorDocumentKind { Scene, Material, AnimationGraph };` 作为 Host 核心。

**采用：**

```text
using EditorDocumentTypeId = std::string; // 或稳定 string_view 常量；如 "Scene", "Material", "AnimClip"

struct EditorDocumentTypeInfo
{
    EditorDocumentTypeId TypeId;
    std::string DisplayName;
    // 资产匹配：扩展名 / AssetClass / 自定义 predicate
    bool (*CanOpenAsset)(const AssetMeta&);
    // 工厂
    std::unique_ptr<EditorDocumentSession> (*CreateSession)(const AssetMeta&, IEditorContext&);
    // 面板套件：本类型拥有的 EditorWindow id 列表，或回调 ApplyVisibility
    PanelSuiteId SuiteId;
    // 策略（类型自己声明，Host 不写死业务）
    bool AllowMultipleSessions = true;     // 同类型多 Session
    bool AllowCloseLastOfType = true;      // 是否允许关掉该类型最后一个
    // …
};

class EditorDocumentTypeRegistry
{
    void Register(EditorDocumentTypeInfo);
    const EditorDocumentTypeInfo* Find(TypeId) const;
    const EditorDocumentTypeInfo* FindForAsset(const AssetMeta&) const;
};
```

- 现有 `SceneEditor` / `MaterialEditor` / `AnimationGraphEditor` 在启动时 **Register**。
- 未来 `AnimationClipEditor`、`PrefabStageEditor` **只注册**，不改 Host 的 switch(kind)。
- 与 `AssetWorkflowModule` 的 Opener 合并或双注册同一 TypeId（避免两套路由）。

**Session：**

```text
class EditorDocumentSession
{
    EditorDocumentId Id;
    EditorDocumentTypeId TypeId;  // 非 enum
    AssetKey Asset;               // 规范身份；用于 OpenOrFocus 去重
    bool Dirty = false;
    CommandStack UndoStack;       // per-session（见 §3.5）
    // 域状态：由类型实现持有（图、轨道、选中…）；Host 不解释
};
```

### 3.3 资产打开契约（明确）

| 场景 | 行为 |
|------|------|
| 打开 **尚未** 打开的资产 | `CreateSession` → 加入 Host → Activate |
| 打开 **已经** 打开的同一资产 | **Activate 已有 Session**（跳转）；不建第二实例 |
| 打开 **同类型、不同资产** | **允许**再建 Session（若 `AllowMultipleSessions`） |
| 类型声明不允许多开 | 由该 TypeInfo 策略处理（替换或拒绝）；**默认允许多开** |

「同一资产」键：优先持久 Guid / AssetId；否则规范化路径。须与 Content Browser、保存路径一致。

### 3.4 Tab UI 与共享面板

- Tab 条：MainMenuBar 下全宽 Host（推荐）。
- 左键：Activate；拖拽：Reorder（Pinned 规则见 §3.4.2）。
- 中键：等价 **Close**（可配置关闭；默认开）。
- Dirty：`UnsavedDocument` 点或标题 `*`；与 OS 标题策略一致。
- 右键：见 **§3.4.1–3.4.4**（完整菜单契约）。
- Console / CB / MainMenu：**Shared suite**；关文档 Tab **不**关它们。

#### 3.4.1 右键菜单总览（对标 + 本引擎）

对照常见产品，再收敛到 Maximum 文档 Tab（**非**面板 Dock Tab）：

| 来源 | 常见项 |
|------|--------|
| **浏览器** | Close / Close Others / Close Tabs to the Right / Close Saved（扩展）/ Mute / Pin / Duplicate / Move to New Window / Reopen Closed |
| **VS Code** | Close / Close Others / Close to the Right / Close Saved / Close All · Pin/Unpin · Split · Copy Path / Copy Relative Path · Reveal in Explorer · Reopen Closed Editor |
| **UE** | 资产编辑器 Tab：Close 为主；另有 Save / Find in Content Browser 等资产向操作 |

本引擎菜单按 **分组** 设计（实现可用 `ImGui` 分隔线）。目标能力一次定清；**Wave** 列表示交付早晚，不是「永远不做」。

#### 3.4.2 菜单项契约

**约定：** 右键落在 Tab *T*（不必先是 Active）。执行前若操作需要前台编辑器，可先 `Activate(T)` 再执行（或操作显式带 `DocumentId`，推荐后者以免闪烁）。

| 分组 | 菜单项 | 行为 | 启用条件 | Dirty / Pin 交互 | Wave |
|------|--------|------|----------|------------------|------|
| **Close** | **Close** | `RequestClose(T)` | 类型允许关（见 `AllowCloseLastOfType` 等） | Dirty → Save / Don't Save / Cancel | **W1** |
| | **Close Others** | 关闭除 *T* 外全部 | 至少还有 1 个其它 Tab | 批量 Dirty 策略（§3.4.3）；**跳过 Pinned** | **W1** |
| | **Close to the Right** | 关闭 *T* 右侧（按当前 Tab 顺序）全部 | 右侧至少 1 个 | 同上；**跳过 Pinned** | **W1** |
| | **Close Saved** | 关闭所有 `!Dirty` 的 Tab | 存在已保存 Tab | 不影响 Dirty；**跳过 Pinned** | **W1** |
| | **Close All** | 关闭全部 Tab | 有 Tab | §3.4.3；**默认不关 Pinned** | **W3** |
| **Pin** | **Pin Tab** | `Pinned=true`；Tab 沉到条左侧 Pin 区 | *T* 未 Pin | Pin 后：不参与 Close Others / Close to the Right / Close Saved；Close All 默认保留 | **W3** |
| | **Unpin Tab** | `Pinned=false` | *T* 已 Pin | — | **W3** |
| **Save** | **Save** | 保存 *T* 对应资产 | 类型支持保存 | 成功后清 Dirty | **W1** |
| | **Save As…** | 另存（若类型支持） | 类型声明支持 | 成功后可更新 AssetKey / 标题 | **W3**（或随资产工作流） |
| **Navigate** | **Copy Asset Path** | 绝对路径进剪贴板 | 有磁盘路径 | — | **W1** |
| | **Copy Relative Path** | 相对 ProjectRoot | 有路径 | — | **W3** |
| | **Show in Content Browser** | CB 选中并滚动到该资产 | CB 可用且资产在注册表 | 不关闭文档 | **W1** |
| | **Reveal in Explorer**（OS 文件夹） | 打开系统文件管理器并选中文件 | 桌面平台 + 有路径 | — | **W3** |
| **Window** | **Move to New Window** | 撕到新 OS 窗 | Viewports / 多窗壳就绪 | **后置**（依赖 Viewports） | **W4+** |
| | **Split Editor** | 并排第二文档组 | Editor Groups 就绪 | **后置** | **W4+** |
| **History** | **Reopen Closed Tab** | 恢复最近关闭的 Session 资产（重新 OpenOrFocus） | 关闭历史非空 | 不恢复未保存缓冲（只重开资产）；Dirty 历史不复活 | **W3** |

**中键 / Tab 上 X：** 与 **Close** 同路径（含 Dirty 确认）。

**拖拽排序与 Pin：**
- Pinned Tab 组成左侧稳定区；Reorder **仅在 Pin 区内或非 Pin 区内**，不可把非 Pin 拖进 Pin 区除非执行 Pin（或拖到 Pin 区＝Pin，产品可选；推荐 **显式 Pin 菜单**，拖拽不隐式 Pin）。
- Close to the Right 的「右」= 当前视觉顺序（含 Pin 区规则下的顺序）。

#### 3.4.3 Dirty 批量关闭策略

Close Others / Close to the Right / Close All 可能连续遇到多个 Dirty：

| 策略 | 说明 | 结论 |
|------|------|------|
| **逐个对话框** | 每个 Dirty 一次 Save/Don't/Cancel | 安全但烦 |
| **汇总对话框** | 「N 个未保存：Save All / Don't Save All / Cancel」 | **推荐默认** |
| **遇 Cancel 中止** | 已处理的保持已关/已存；未处理的保留 | **必须** |

Pinned 默认 **不在批量关闭集合内**（对齐 VS Code sticky）。若用户 Close 单个 Pinned Tab，仍允许（与 VS Code「菜单可关 sticky」类似），须 Dirty 确认。

#### 3.4.4 类型扩展菜单（可选钩子）

Host 提供固定分组后，允许 DocumentType 追加 **类型专有项**（避免 Host 写死业务）：

```text
// 例：Material → "Open Preview Settings"
// 例：Scene → "Play From Here"（若有）
TypeInfo.AppendTabContextMenu(session, menu);
```

规则：专有项放在 **Save** 与 **Navigate** 之间或末组；不得覆盖 Host 保留命令 Id。

#### 3.4.5 键盘（与菜单同命令，非必须同期 UI）

| 命令 | 建议快捷键（可改） | Wave |
|------|-------------------|------|
| Close Active | Ctrl+W | W1 |
| Close Others | —（菜单为主） | W1 |
| Save Active | Ctrl+S（已有则绑 Active Session） | W1 |
| Reopen Closed | Ctrl+Shift+T（浏览器习惯） | W3 |
| Next / Prev Tab | Ctrl+Tab / Ctrl+Shift+Tab | W3 |

### 3.5 CommandStack：按 Session 分（硬）

| 规则 | 说明 |
|------|------|
| 每 Session 一个 `CommandStack` | 编辑只进 Active（或显式目标）Session 的栈 |
| Activate 切换 | 当前 Undo/Redo 绑定 ActiveSession 的栈 |
| Close Session | 丢弃该栈（或提示；无跨 Session 合并） |
| 全局栈 | **删除**作为多文档默认；迁移期可短暂双轨但须删干净 |

与 [EDITOR_COMMAND_HISTORY](./EDITOR_COMMAND_HISTORY.md) 对齐时更新该文：栈的所有者改为 Session。

### 3.6 子编辑器 / SubModule 关系

- `EditorSubModule`（或演进名）= **某 DocumentType 的实现者**：创建 Session、Tick 激活态、提供面板套件、保存/脏。
- Host **不**再「互斥 Activate = 销毁另一类型会话」；Activate = 前台 Session + 套件可见性。
- 多 Session 同类型：类型实现必须支持 **多实例状态**（`map<DocumentId, State>`），禁止模块级单槽覆盖。

### 3.7 与 Scene / Play Mode

- Host **不**写死「全局只能一个 Scene」。
- 若 Play Mode / 双 Scene 世界短期要求「编辑用 Scene 仅一个」，由 **Scene 类型的 `AllowMultipleSessions=false`**（或「第二个打开即替换并确认」）声明——这是 **类型策略**，不是 Host 枚举特例。
- 推荐中期：允许多 Scene Session（只是未 Active 的不驱动 Play）；若近期实现成本高，Scene 可先 `AllowMultipleSessions=false`，其它类型 true。

### 3.8 ImGui

同前：Reorderable TabBar、UnsavedDocument、ContextMenu 均足够；撕窗 / 每 Session DockSpace 后置。

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| enum kind + switch | **拒绝**（扩展侵入） |
| 方案 B 一上来 | **后置**；先 A + 类型套件 |
| 同资产允许多 Session | **拒绝**（与「跳转已打开」冲突）；除非强制「克隆视图」另模式 |
| 全局 Undo | **拒绝** |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 类型实现仍单槽 | DoD：两 Material 同时脏切回均在；代码审查 map 槽 |
| 套件显隐误 ResetLayout | Activate 默认不 Reset；仅用户 Reset 或首次 |
| TabBar 同帧抢焦点 | OpenOrFocus 已 Activate 后，旧 Tab 仍 `selected` 时不得再 Activate；仅 `IsItemActivated` 响应点击 |
| 多预览 RT | 仅 Active Tick；Close 释放 |
| 注册表与 Workflow 双路由 | 单一 TypeId / 单一 OpenOrFocus 入口 |
| Scene 多开与 Play | 类型策略显式；测 Play 只绑约定 Scene |

---

## 6) 验收

- [ ] 无 Host 内业务 enum；新类型可 Register 接入 Tab
- [ ] 同类型不同资产可同时多 Session；同资产再开 → 聚焦已有
- [ ] 每 Session 独立 Undo；切 Session 后 Undo 不串台
- [ ] Active=AnimGraph/未来 AnimClip 时，其专有面板可见，Scene 专有面板隐藏（套件切换）
- [ ] Shared Console 不随文档关
- [ ] Tab 排序/关闭/Dirty；右键 **W1 组**可用（Close / Close Others / Close to the Right / Close Saved / Save / Copy Path / Show in CB）
- [x]（W3）Pin 语义：批量关闭跳过 Pinned；Reopen Closed 可用  
- [x] Editor 构建 + 手测记 Progress（Reveal / Pin 直关 / Tab 图标跟进已过）  

---

## 7) Status note

**W3（2026-09-15）：** Pin/Unpin + 左侧 Pin 区排序；Close All（跳过 Pinned）；Reopen Closed + Ctrl+Shift+T；Ctrl+Tab 切 Tab；Copy Relative / Reveal in Explorer（`SHOpenFolderAndSelectItems`）；Tab FA 类型图标 + Pin `*`；`AppendTabContextMenu` 钩子已挂。  
**W3 余量：** Save As…；Scene `AllowMultipleSessions` 产品决策（现状 false）；汇总 Dirty 对话框（仍逐 Tab）。

---

## 8) 交付波次

| Wave | 内容 |
|------|------|
| **W0** | `DocumentTypeRegistry` + `Session` + `Host` API；现有三类型注册；OpenOrFocus 去重；多实例状态升槽（可先 Material） |
| **W1** | Tab UI + Dirty；右键 Close 组 + Save / Copy Path / Show in CB；Activate 套件显隐；**禁止无条件 ResetLayout**；**Shared CB 进各类型默认布局**；Window **Focus Document**（非 Mode） |
| **W2** | **per-Session CommandStack** 切满；删全局默认栈路径 |
| **W3** | **Pin/Unpin**；Close All；Copy Relative / Reveal in OS；**Reopen Closed**；类型 `AppendTabContextMenu`；Tab 图标；Scene 多开策略落地 |
| **W4+** | Move to New Window / Split（Viewports、Editor Groups）；方案 B 评估 |

---

## 9) 开放点

| # | 问题 | 推荐默认 |
|---|------|----------|
| O1 | 布局 A vs B | **A（共享壳 + 类型套件）** |
| O2 | Scene 是否允许多 Session | **类型策略**：短期可 `false`；中期 `true` |
| O3 | 资产键 | **Guid/AssetId 优先**，路径回退 |
| O4 | Tab 条位置 | **MainMenuBar 下全宽** |
| O5 | TypeId 表示 | **稳定字符串 Id**（与模块 Id 可对齐） |
| O6 | 批量 Dirty 关闭 UI | **汇总对话框**（Save All / Don't Save All / Cancel） |
| O7 | Close All 是否关掉 Pinned | **默认不关 Pinned** |
| O8 | 拖到 Pin 区是否隐式 Pin | **否**；仅菜单 Pin/Unpin |

---

## 10) ID 备注

| ID | 角色 |
|----|------|
| `ED-F09` | Console 过滤 |
| `ED-F10` | Settings（过滤/布局持久化） |
| **`ED-F11`** | 本 Feature |
| `ED-F12`（建议） | Query–Modify–Verify |
| `RND-F17`（建议） | 隔离预览 RT |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-13 | 初稿（MVP + enum kind + 方案 A 简述） |
| 2026-09-13 | **修订：** 完整产品目标；**注册式 DocumentType**；资产多开/聚焦契约；**CommandStack per Session 硬性**；澄清方案 A=共享 Dock 壳+类型面板套件（AnimClip Track 等） |
| 2026-09-13 | **补：** §3.4 文档 Tab **右键菜单完整契约**（分组、启用、Dirty/Pin、类型钩子、快捷键、Wave） |
| 2026-09-13 | **§3.1.1：** Mode → Active Session / Shared vs Type Suite；CB 须进各类型默认布局；Window Focus 语义 |
