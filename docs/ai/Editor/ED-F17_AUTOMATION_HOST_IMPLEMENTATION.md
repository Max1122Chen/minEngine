# ED-F17 — Automation Host + Q/E/V Surface — Implementation Plan

## Meta
- **ID:** `ED-F17`
- **Status:** Done
- **Last updated:** 2026-09-18（S04 收口）
- **Related:** [Design Spec](./ED-F17_AUTOMATION_HOST_DESIGN.md) · [CLI-F02](../Platform/CLI/CLI-F02_AUTOMATION_LAUNCH_DESIGN.md) · [MCP-F02 schema](../../../schemas/automation/v1/) · [MCP-F01 §6](./MCP/MCP-F01_ARCHITECTURE_DESIGN.md)

## TL;DR

Maximum 内 Automation Host（Named Pipe + length-prefix）+ 同源 Surface/域服务。按 MCP-F01 切 **A → B → C**；GUI/Debug 全量收敛属 **ED-F18**（不挡 Done）。**S01–S04 Done（Scene MVP Host P0-complete）。**

## Scope
- **In:** `Editor/src/Automation/`；Pipe Listen；ready-file 不变量；主线程分派；CapabilityRegistry；Scene MVP ops（经 IScene*Service）；`editor.ping` / `get_state` / `get_capabilities`
- **Out:** MCP Server；ED-F18 GUI 改调；Guid `--open-scene`；完整 Validation

## Reader quick start
1. Design §3.2–3.4
2. 下表切片（对齐 MCP-F01 ED-F17-A/B/C）
3. CLI-F02 getters：`Editor::GetAutomationPipeName` / `GetAutomationReadyFilePath`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `ED-F17-S01` | Pipe server + Host 生命周期；Listen → ready-file；帧读写 | **Done** | ready-file 冒烟；Win NamedPipe 客户端往返 |
| `ED-F17-S02` | CapabilityRegistry + Dispatcher + Surface；`ping` / `get_state` / `get_capabilities` | **Done** | ping + get_state JSON ok |
| `ED-F17-S03` | IScene*Service（薄）+ `get_property` / `set_property` / `save` + validate stub | **Done** | get/set `Transform.Position`；session 栈 Command |
| `ED-F17-S04` | 铺满 Scene MVP 其余 op；文档收口 Feature Done | **Done** | hierarchy/object/create/delete/add/remove/reparent + validate stub；Pipe 手测 PASS |

## 2) 切片详情

### ED-F17-S01 — Pipe + Host + ready-file

- **Goal:** 传输层可测；不变量硬保证。
- **Touch:** `Editor/src/Automation/`
  - `AutomationPipeServer`（CreateNamedPipe；u32 LE + UTF-8 JSON）
  - `AutomationHost`（后台 Accept/读；`Tick` 主线程；Shutdown）
  - Editor：`Initialize` 后若有 pipe → `Start`；`Run` 调 `Tick`；`Shutdown` 停 Host
- **Pipe 名：** CLI 传入；若无 `\\.\pipe\` 前缀则补上
- **Ready：** Listen 成功 **且** 项目已打开（含可选 open-scene）后写 `AutomationReadyFilePath`；无 ready 路径则仅日志
- **DoD:**
  - [x] Listen 失败 → 启动失败（exit 1 路径）
  - [x] ready-file 内容含 pipe 名（单行或短文本即可）
  - [x] 连接后可读一帧请求、写一帧响应（可先 echo / UnknownOp）
- **Verify:** PowerShell/.NET NamedPipeClient + 手写 length-prefix；或临时小工具

### ED-F17-S02 — Registry + ping / get_state / capabilities

- **Goal:** 协议信封可分派；冷启动拿 session。
- **Touch:** `CapabilityRegistry` · `AutomationDispatcher` · `EditorAutomationSurface` · `IEditorAutomationSurface`
- **Ops:** `editor.ping` · `editor.get_state` · `editor.get_capabilities`
- **sessionId wire：** `sess-<EditorDocumentId.Value>`（稳定数字 id）；`defaultSessionId` = 首个 Scene session 或 Active Scene
- **DoD:**
  - [x] KindOpMismatch / UnknownOp / ProtocolVersionMismatch
  - [x] get_state 形状对齐 MCP-F02 §3.4.0
  - [x] capabilities 由 C++ 注册表生成
- **Verify:** 客户端三连请求

### ED-F17-S03 — Scene P0-alpha services

- **Goal:** 同源 Edit 包装点落地（GUI 暂不改 → ED-F18）。
- **Touch:** `Services/Scene/ISceneAutomationServices` · `SceneAutomationServices` · `ScenePropertyValueCodec`
- **Ops:** `scene.get_property` · `scene.set_property` · `scene.save` · `scene.validate`（stub）
- **Value JSON：** 反射路径 + `Transform.*`→`m_Transform.*` 归一
- **DoD:**
  - [x] set_property → EditorCommand 入 **session** 栈（非 Active Tab 默认）
  - [x] Undo 有效（经 Command 栈）
- **Verify:** Pipe 改属性；Editor Undo

### ED-F17-S04 — Scene MVP 铺满 + 收口

- **Goal:** P0-complete Host 侧 op 表；Feature Done。
- **Ops:** `get_hierarchy` / `get_object` / `create_game_object` / `delete_game_object` / `add_component` / `remove_component` / `reparent`（+ S03 已有）
- **Notes:** `add_component` 接受全名或 unambiguous 短名（如 `StaticMeshComponent` → `minEngine::StaticMeshComponent`）
- **DoD:**
  - [x] Design 验收勾选
  - [x] Registry Done
  - [x] ACTIVE_WORK → MCP-F03
- **Verify:** Pipe 手测：hierarchy → create → add_component → reparent → remove_component → validate stub → delete

## 3) 依赖顺序

```text
S01 → S02 → S03 → S04
         └─ MCP-F03 可在 S02 后并行起 mock；端到端需 S03+
```

## 4) 延后

| Item | Reason |
|------|--------|
| GUI/Debug 改调域服务 | ED-F18 |
| 多客户端并发 | MVP 串行单飞 |
| TCP 载体 | 换传输不改 JSON |

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 主线程卡顿 | 串行单飞；长 op 后置 |
| value JSON 漂移 | S03 前锚定；钉 examples |
| Pipe 名格式 | 自动补 `\\.\pipe\` |

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | Impl 开工；对齐 ED-F17-A/B/C |
| 2026-09-18 | S01–S02：Pipe Host + ready-file；ping/get_state/get_capabilities；手测 PASS |
| 2026-09-18 | S03：SceneAutomationServices + JSON↔blob codec；get/set_property/save/validate stub；set 经 session Command 栈 |
| 2026-09-18 | S04：其余 Scene MVP op + Surface 接线；className 短名解析；Pipe 手测 PASS；**Status → Done** |
| 2026-09-18 | Surface handler map 路由；Design/Host 写明 single in-flight（非 queue） |
