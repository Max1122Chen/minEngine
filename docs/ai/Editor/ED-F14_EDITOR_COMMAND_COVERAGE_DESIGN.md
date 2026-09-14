# ED-F14 — 各 SubEditor 命令面完备 — Design Spec

## Meta
- **ID:** `ED-F14`
- **Type:** Feature
- **Status:** Planned（Design 首稿；**依赖 ED-F13 收口后再开实现**）
- **Owner:** project maintainer
- **Last updated:** 2026-09-14
- **Branch:** `feat/editor`
- **Depends on:**
  - **`ED-F13` Done**（Command-first 形态：无 Apply 套壳）
  - `ED-F12` Done（Debug / EditorCommand 协议骨架）
  - `ED-F11` Session 栈（已有）
- **Related:**
  - [ED-F13 Command-first](./ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md)
  - [ED-F12 Agent Edit Protocol](./ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md)
  - Material / AnimGraph 既有 Editor 设计（按打开资产类型扩展命令集）
- **Blocks:** MCP 富工具（软）；跨域 Agent 竖切完整度
- **Philosophy:** 同一套 EditorCommand；按 **资产/Session 类型** 扩展命令面，不另造 Agent Framework

## TL;DR

在 F13 把 Scene 的「套壳」拆干净之后，本 Feature **按 SubEditor / 文档类型补齐应有的 EditorCommand**，并可选接上 Debug 动词（`edit` / `verify` / …）。  
目标：每个已支持的编辑会话，人类 GUI 与 Debug/MCP **走同一命令集**，而不是各域私自改内存。

---

## Pre-flight（2026-09-14）

| 项 | 结论 |
|----|------|
| 扫描 | Scene 已有一批 `Editor*Command`（待 F13 整形）；Material / AnimGraph **编辑未入 Session 栈** |
| 前置 | **partial** — 必须等 **F13 Done**，否则新命令会继续抄 Apply 套壳 |
| 债风险 | **medium** — AnimGraph 事件多；Material 预览与脏标记 |
| WIP | 与 F13 串行；勿并行开码 |
| 建议 | **Defer 实现** 至 F13；本文件先定 **覆盖矩阵与分期**，审批后锁定 |

---

## Scope

### In
- 盘点 **当前已注册文档类型** 的编辑操作 → 哪些已是 EditorCommand、哪些是直接变异
- 为缺口建立 **EditorCommand**（遵循 F13 契约：显式目标、直接改对象/资产）
- Scene：F13 之后仍缺的常用操作（若有）补齐
- Material / AnimationGraph：第一批可撤销编辑命令（MVP 子集，见 §3）
- Debug 前端：在已有 EditorCommand 上挂 **基线/方言** 字符串（含 F12 推迟的 `edit` / `verify`；`invoke` 另估）
- 每域至少一条自动化或固定手测清单

### Out
- 重做 F13（Apply 删除）
- 完整 Anim Graph 一切操作的 undo（可分期；本期 MVP）
- 全 MCP schema 产品化（`MCP-F01`）
- Prefab / 未落地文档类型
- 把查询全部对象化为 EditorCommand（可附带小切片，非必须一次做完）

---

## Reader quick start

1. 先读 [ED-F13](./ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md) 契约
2. 本文件：覆盖矩阵 + Wave
3. 实现时每域先「最小可撤销竖切」，再扩命令表

---

## 1) 背景与目标

F12 定了「一种 Command、多种前端」。  
F13 定了「Command 不是 Editor 函数套壳」。  
F14 补 **产品完整度**：打开 Scene / Material / AnimGraph 时，该域常见编辑都应能：

1. 由 GUI 构造 Command 入 Session 栈  
2. 由 DebugCommand（及日后 MCP）构造 **同一** Command  
3. Undo/Redo 行为一致  

---

## 2) 现状覆盖矩阵（首稿 · 随实现更新）

### 2.1 Scene（`SceneEditor`）

| 能力 | GUI→Command | Debug 文本 | F14 备注 |
|------|-------------|------------|----------|
| Add/Remove Empty GO | 有（待 F13 整形） | 部分 / 查现网 | 补齐缺口与对称性 |
| Delete GO + restore | 有 | 查现网 | F13 后回归 |
| Add/Remove Component | 有 | 查现网 | F13 去选中耦合后接 Debug |
| Rename GO/Comp | 有 | 查现网 | |
| Move Comp / Reparent | 有 | 查现网 | |
| Transform | 有 | 查现网 | |
| Set property | 有（`set`） | 有 | **`edit`（守 specifier）** 本 Feature |
| Get / find / list | 查询路径 | 有 Payload | 可选对象化 |
| Multi-select ops | 弱/无 | 无 | 可 Deferred |
| Duplicate / Paste | ? | 无 | 评估后列入 Wave |

### 2.2 Material（`MaterialEditor`）

| 能力 | 现状 | F14 MVP |
|------|------|---------|
| 改标量/枚举参数 | 直接写 Session + preview | `EditorSetMaterialParameterCommand`（名待定） |
| 保存 | 菜单绑定 | 保持；不必强行 Command |
| Undo | **无栈** | 至少参数类可撤销 |
| Debug | 弱 | `get`/`set`/`edit` 方言或基线 Guid 路径 |

### 2.3 AnimationGraph（`AnimationGraphEditor` + SmBridge）

| 能力 | 现状 | F14 MVP |
|------|------|---------|
| 拖节点 / 加边 / 改条件等 | `ApplyEditEvents` 直接改图 | 将 **可持久化** 事件落为 EditorCommand（可一批合并） |
| Entry/AnyState 装饰位移 | Session-only，故意不 dirty | **保持不入 undo 或单独策略**（与现注释一致） |
| Undo | **无栈** | MVP：状态增删、转移增删、参数改 的可撤销子集 |
| Debug | 弱 | 后置；先 GUI Command |

### 2.4 Debug 动词（承接 F12）

| 动词 | 意图 | F14 |
|------|------|-----|
| `edit` | 守 Visible/Edit specifier | **In** |
| `verify` | 断言属性/状态 | **In**（最小：property equals） |
| `invoke` | 调反射方法 | **评估**；高风险则 Deferred |

---

## 3) 方案

### 3.1 原则

1. **先 F13 契约，再扩面** — 新命令禁止引入 `*Editor::Apply*` 套壳。  
2. **按文档类型注册命令集** — 与 Session `typeId` 对齐（F11/F12）。  
3. **MVP 竖切** — 每域先「改一条可持久化数据 + undo」，再铺表。  
4. **Debug 后挂** — 先有 EditorCommand，再写字符串解析；避免又造平行语义。

### 3.2 推荐 Wave

| Wave | 内容 |
|------|------|
| **W0** | F13 Done 后门禁；刷新 §2 矩阵（以代码为准） |
| **W1** | Scene 缺口 + `edit` / `verify` Debug |
| **W2** | Material 参数类 EditorCommand + 栈 + 最小 Debug |
| **W3** | AnimGraph 可持久化编辑事件 → Command（子集） |
| **W4** | 文档/Registry Done；剩余列入 Deferred 表 |

### 3.3 与 F13 的分工（避免抢活）

| 工作 | Feature |
|------|---------|
| 删 Scene Apply\*、去 Select 前置 | **F13** |
| 新命令、新域、Debug 新动词 | **F14** |
| 半吊子「Material 仍直接写但挂空 Command」 | **禁止** |

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. F13 后按域扩 Command | 契约清晰 | 总周期长 | **选用** |
| B. 与 F13 并行给 Material 先加套壳 Command | 表面上有 undo | 复制反模式 | 否 |
| C. 只做 Debug 动词、不碰 Material/AnimGraph | 快 | 违背「各 Editor 完备」 | 否（可作为 F14 内再砍 Wave，但不改目标） |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| AnimGraph 事件粒度与合并 | undo 体验差或栈爆炸 | MVP 合并为「一次手势一个 Command」；装饰节点不入栈 |
| Material preview 与 Command 时序 | 预览不同步 | Command 成功后统一 `ApplySessionToPreview`（UI 钩子，非套壳变异） |
| `invoke` 安全面 | 误调危险 API | 默认 Deferred；白名单另议 |
| 范围变成「所有编辑器完美」 | 无法收口 | §2 矩阵 + Deferred 列；W4 强制停 |

---

## 6) 验收标准

- [ ] §2 矩阵每个「F14 MVP」格有对应 EditorCommand（或显式 Deferred 行）
- [ ] Material / AnimGraph 至少一类编辑走 Session `CommandStack` 且可 Undo
- [ ] Debug：`edit` + `verify` 最小可用；与 GUI 同一 Command 类型
- [ ] 无新增 `SceneEditor::Apply*` 式套壳
- [ ] 测试或手测清单写入 Progress；Registry → Done

---

## 7) Status note

**Blocked on：** `ED-F13` 实现完成（删除 Apply 套壳）。  
F13 未 Done 前本 Feature 保持 Planned，不开码。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-14 | 首稿：覆盖矩阵 + 依赖 F13；edit/verify 纳入 |
