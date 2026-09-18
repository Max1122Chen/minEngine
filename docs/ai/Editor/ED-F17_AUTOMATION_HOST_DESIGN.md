# ED-F17 — Editor Automation Host + 同源 Q/E/V Surface — Design Spec

## Meta
- **ID:** `ED-F17`
- **Type:** Feature
- **Status:** Done
- **Last updated:** 2026-09-18（S04 Done；handler map + 单飞调度不变量写明）
- **Branch:** `feat/mcp`
- **Depends on:**
  - `MCP-F02`（协议形状；帧定界已拍板）
  - `CLI-F02`（pipe / ready-file）
  - `ED-F12`–`F14`（EditorCommand / Scene）
- **Related:**
  - [Implementation](./ED-F17_AUTOMATION_HOST_IMPLEMENTATION.md)
  - [MCP-F01 §3.4–3.5 同源模型](./MCP/MCP-F01_ARCHITECTURE_DESIGN.md)
- **Blocks:** MCP-F03 端到端

## TL;DR

Maximum 内提供两件事：（1）**同源域服务**（Scene Query/Edit/Verify…），GUI / Debug / Automation **共用**；（2）**Automation Host** 把 Named Pipe 上的协议请求调度到这些服务。Edit 服务内部才 `new EditorCommand` 入 Session 栈——**不为 MCP 另包命令**。

## Scope
- **In:** 域服务抽取/门面、Host、Pipe、主线程调度、Scene MVP 的 Q/E/V、与现有 Command 工厂对齐
- **Out:** MCP SDK、粗 Tools、Material/Anim 完备、完整 Validation 引擎

## Reader quick start
1. [MCP-F01](./MCP/MCP-F01_ARCHITECTURE_DESIGN.md) §3.4–3.5
2. 本文件 §3.1–3.3（包装边界）
3. §3.5 Scene 清单

---

## 1) 背景与目标

### 维护者意图（同源）

GUI、DebugCommand、minEngine MCP Server 对引擎而言应是 **同一套底层操作**。差别只在「意图怎么说清楚」，不在「改世界的那一刀」。

### 成功标准

- 无 MCP 时，小客户端经 Pipe 调 `scene.set_property`，与 Inspector 改同一属性产生 **同类 Undo**。
- Debug `set` 与协议 Edit 最终调用 **同一** `ISceneEditService.SetProperty`（或等价工厂）。

---

## 2) 现状

| 组件 | 状态 | 缺口 |
|------|------|------|
| EditorCommand / Scene 命令 | 有 | GUI/Debug 未必都经统一服务方法 |
| Document Session + 栈 | 有 | OK |
| 统一 Query/Verify 服务 | 弱 | 需显式化 |
| Named Pipe Host | 无 | 本 Feature |
| 协议 → 域服务 | 无 | 本 Feature |

---

## 3) 方案

### 3.1 包装边界（回答「EditorCommand 包到哪」）

```text
推荐包装点 = 域 Edit/Query/Verify 服务（L1）
            不是「MCP 专用 Command 子类」

ISceneEditService.SetProperty(...)
    ├── 校验 session / object / path
    ├── new EditorSetObjectPropertyCommand(...)
    └── session.GetCommandStack().Execute(...)

GUI / Debug / AutomationHost.Dispatcher
    └── 只调用 ISceneEditService（或 Surface 转调）
```

| 层级 | 职责 | 是否「MCP 包装」 |
|------|------|------------------|
| EditorCommand | Undo 单元 | 否；既有机制 |
| 域服务 | **同源 API**；组装 Command | **是真正的包装点**（对所有前端） |
| Automation Surface | op 字符串 → 域服务 | 薄分派 |
| Host | Pipe + 线程 | 传输 |
| MCP Server | 粗 Tool → JSON | 进程外适配 |

**禁止：** Host 或 TS 内复制一套「改 Transform」逻辑。

### 3.2 模块边界

```text
Editor/
  Services/                  # 或 Automation/Domain/
    Scene/
      ISceneQueryService
      ISceneEditService      # 同源 Edit
      ISceneVerifyService
  Automation/
    AutomationHost
    AutomationPipeServer     # length-prefix JSON 帧
    AutomationDispatcher     # kind+op → IEditorAutomationSurface
    EditorAutomationSurface  # 实现 IEditorAutomationSurface（门面名已拍板，不用 *Interface）
    CapabilityRegistry       # 能力真源；供 get_capabilities
```

**命名：** 门面用 **`IEditorAutomationSurface`**（否决 `IEditorAutomationInterface`，避免与 `I*` 叠词）。域能力用 `IScene*Service`。

MVP 允许：先把现有 `SceneEditor::ApplySetObjectProperty` 等收进 `ISceneEditService`，Host 先用；**GUI/Debug 全量改调服务 → [ED-F18](./ED-F18_GUI_DEBUG_DOMAIN_SERVICE_CONVERGENCE_DESIGN.md)（Planned 挂账）**。

### 3.3 执行模型

| kind | 路由 | Undo |
|------|------|------|
| query | `ISceneQueryService` / editor state | 否 |
| edit | `ISceneEditService` → Command | 是（save 等除外） |
| verify | `ISceneVerifyService` | 否 |

- IO 在后台线程；**执行在 Editor 主线程**。
- **调度不变量（当前 MVP，故意为之）：单飞（single in-flight）。**
  - Host 仅有一个 `PendingExchange` 槽；IO 读完一帧后 **阻塞** 等主线程 `Tick` 分派并写回响应，再读下一帧。
  - **不是**请求队列；同连接严格串行；跨客户端也不并行改 Scene。
  - Surface 路由：`op → handler map`（与 CapabilityRegistry 分离：后者是对外契约，前者是对内分派）。
  - **升级触发（有证据再做）：** Agent 连打细 op 被帧率拖死、长 op 堵死管道、或产品需要 batch/多客户端——再考虑有界 queue + 每 Tick drain N。
- **禁止** Active Tab 作为默认写目标；必须 `sessionId`。

### 3.4 生命周期 / Ready

**不变量（硬）：**

```text
Listen(pipe) 成功 →（若请求则 open-scene 完成）→ 写 ready-file
```

禁止先写 ready-file 再 Listen。

1. `--automation-pipe` → Listen。  
2. 项目与目标 Scene Session 就绪 → 写 **ready-file**（CLI-F02）；可选 stdout 行。  
3. 请求循环；退出时失败码 `EditorShuttingDown`。

Pipe 名建议含 **pid 或随机后缀**（本机信任模型，见 MCP-F01 §4.6）。

内部连通检查（非 Agent Tool）可用于 Attach 后确认 Host 活着。

分派时：**op 查注册表**；请求 `kind` 不一致 → `KindOpMismatch`。

### 3.5 Scene MVP op → 服务

| op | 服务方法 |
|----|----------|
| `editor.get_capabilities` | CapabilityRegistry.BuildCapabilities |
| `editor.get_state` | 会话/项目摘要（非 Scene 域也可放 EditorShellQuery） |
| `scene.get_hierarchy` | Query.GetHierarchy |
| `scene.get_object` | Query.GetObject |
| `scene.get_property` | Query.GetProperty |
| `scene.set_property` | Edit.SetProperty |
| `scene.create_game_object` 等 | Edit.* → 现有 Command 类 |
| `scene.save` | Edit.SaveDocument（non-undo） |
| `scene.validate` | Verify.Validate（**P0 stub**：诚实声明；见 MCP-F02） |

内部 `editor.ping`：仅 Host/连接管理使用，**不**作为产品 Tool。

**值编解码：** ED-F17-B 前做短 spike，复用现有 JSON/反射序列化；结论写入 Implementation / Progress。

### 3.6 传输

- Named Pipe；帧 = **u32 LE length + UTF-8 JSON**（与 MCP-F01/F02 一致）。
- Pipe 名：调用方传入全名；默认建议 `\\.\pipe\minEngine.Maximum.<pid>`（**须含 pid/随机**，勿固定全局名）。

### 3.7 错误码（摘要）

`UnknownOp` · `KindOpMismatch` · `ProtocolVersionMismatch` · `InvalidPayload` · `SessionNotFound` · `InvalidObjectId` · `InvalidPropertyPath` · `InternalError` · `EditorShuttingDown` · `Timeout`  
分类见 MCP-F02 §3.1.2。

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| Host 内直接 new Command、GUI 另走一套 | **否决为终态**（可短接但须收敛） |
| Debug 字符串当协议真源 | **否决** |
| 无头 Automation.exe | 后置 |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| GUI 未改走域服务 → 假同源 | **ED-F18 挂账**；本 Feature 只保证 Host 路径 |
| 服务变成上帝类 | 按域拆 IScene*；Surface 只分派 |
| ready 竞态 | Listen→ready-file 不变量 |
| value JSON 漂移 | 序列化 spike |

---

## 6) 验收标准

- [x] Pipe + length-prefix 可往返 JSON
- [x] `Listen` 成功之后才出现 ready-file
- [x] `scene.set_property` 经 **ISceneEditService** 入栈；Undo 有效
- [x] `editor.get_state` 返回含 `sessions` / `defaultSessionId`（open-scene 后）
- [x] `get_capabilities` 由 **CapabilityRegistry**（C++）生成；非 TS 独有手写真源
- [x] 文档写明：GUI/Debug 全量收敛属 **ED-F18**，不挡本 Feature Done
- [x] Kind/op 不一致返回 `KindOpMismatch`

---

## 7) 仍可议

1. ~~域服务放 `Editor/src/Services/Scene/` 还是 `Automation/Domain/`？~~ → **已选** `Services/Scene/`（与现有 Editor Services 并列；Host 只调度）。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-17 | Draft：Host / Q/E/V |
| 2026-09-17 | 同源包装边界；帧定界/ready 对齐 |
| 2026-09-17 | **评审修补：** ready 不变量、DoD 与 ED-F18 解耦、KindOpMismatch、capabilities 真源；待审批 |
| 2026-09-18 | S01–S04 落地；Scene MVP op 全表经域服务；**Status → Done** |
| 2026-09-18 | Surface：`op → handler map`；Design §3.3 钉死单飞（非 queue）与升级触发 |
