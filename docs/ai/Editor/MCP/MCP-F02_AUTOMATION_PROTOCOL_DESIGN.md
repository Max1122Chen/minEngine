# MCP-F02 — Automation Protocol（JSON 线协议）— Design Spec

## Meta
- **ID:** `MCP-F02`
- **Type:** Feature
- **Status:** Done（契约层：schema + examples + ERROR_CODES；待 Feature 审批后开下游）
- **Last updated:** 2026-09-17
- **Depends on:** `MCP-F01`（边界）；语义对齐 Editor Surface + `ED-F12`（Edit）
- **Related:**
  - [Implementation](./MCP-F02_AUTOMATION_PROTOCOL_IMPLEMENTATION.md)（**Done**）
  - Schema: [`schemas/automation/v1/`](../../../../schemas/automation/v1/)
  - [MCP-F01 Architecture](./MCP-F01_ARCHITECTURE_DESIGN.md)
  - [ED-F17 Automation Host](../ED-F17_AUTOMATION_HOST_DESIGN.md)
  - [MCP-F03 MCP Server](./MCP-F03_MCP_SERVER_DESIGN.md)
- **Blocks:** ED-F17 / MCP-F03 实现

## TL;DR

定义 **与 MCP 解耦** 的 JSON 线协议：版本、requestId、`query|edit|verify`、显式 Session/Guid/PropertyPath、结构化错误。细 `op` 映射到 Maximum 内 **Editor Q/E/V Surface**（Edit 常落到 EditorCommand）。MCP 粗 Tools 在 Server 侧展开为细 op——协议层保持可测、可扩展。

## Scope
- **In:** 信封格式、版本策略、错误模型、Scene MVP **细 op** 目录与载荷形状、身份约定、与粗 Tool 的分工
- **Out:** MCP Tool 外观（属 MCP-F03）；C++ Host（属 ED-F17）

## Reader quick start
1. [MCP-F01](./MCP-F01_ARCHITECTURE_DESIGN.md) §0–§3（Q/E/V、粗 Tool）
2. §3.1 信封
3. §3.3 Scene 细 op 表

---

## 1) 背景与目标

MCP Server 与 Maximum 之间需要稳定、可测试的 IPC 载荷。协议必须：

- 可在无 MCP 的情况下用脚本测 Host；
- 升级时用 `protocolVersion` 门闸；
- 错误可机器消费（code + message + 可选 path/objectId）。

---

## 2) 现状

- DebugCommand `PayloadJson` 已有部分 JSON 习惯，但面向人类 REPL，**不是**跨进程协议。
- 尚无独立 wire schema / 版本号。

---

## 3) 方案

### 3.1 请求信封（草案）

```json
{
  "protocolVersion": 1,
  "requestId": "req-001",
  "kind": "query" | "edit" | "verify",
  "op": "scene.get_property",
  "sessionId": "…",
  "payload": { }
}
```

| 字段 | 规则 |
|------|------|
| `protocolVersion` | 整数；不匹配 → 立即失败 |
| `requestId` | 客户端生成；响应原样回显 |
| `kind` | `query` / `edit` / `verify`；**冗余校验字段**（见 §3.1.1） |
| `op` | 细操作名；**主键**；稳定字符串；`domain.action` |
| `sessionId` | **edit** 必填；query/verify 按 capabilities.`sessionRequired` |
| `payload` | op 专用对象 |

**与粗 Tool 分工：** Agent 调 `editor_edit(domain, action, …)`；MCP Server 填 `kind=edit` + `op="{domain}.{action}"`。

### 3.1.1 `op` 与 `kind`（已拍板）

| 规则 | 说明 |
|------|------|
| **`op` 为主键** | Host 按 `op` 查 CapabilityRegistry 并分派 |
| **`kind` 必须一致** | 请求 `kind` 须等于该 op 在注册表中的 `kind`；否则 **`KindOpMismatch`**，不执行 |
| MCP 组装 | 粗 Tool 决定期望 kind；拼出的 op 须属于该 kind（可用已拉 capabilities 校验） |

禁止：仅信 `kind` 而忽略注册表；禁止 `kind=edit` 却执行 query 类 op。

### 3.1.2 错误码分类（P0）

| 类别 | code 例 | 含义 |
|------|---------|------|
| 协议/客户端 | `ProtocolVersionMismatch` · `KindOpMismatch` · `UnknownOp` · `InvalidPayload` | 可改请求重试 |
| 会话/目标 | `SessionNotFound` · `InvalidObjectId` · `InvalidPropertyPath` | 换 session/目标 |
| 内部 | `InternalError` · `EditorShuttingDown` · `Timeout` | 勿盲目重试同一写 |

### 3.2 响应信封（草案）

```json
{
  "protocolVersion": 1,
  "requestId": "req-001",
  "ok": true,
  "result": { },
  "errors": [],
  "warnings": []
}
```

错误项草案：

```json
{
  "code": "InvalidObjectId",
  "message": "No object with this GUID in session scene.",
  "objectId": "…",
  "propertyPath": "…"
}
```

### 3.3 身份约定

| 概念 | Wire 字段 | 说明 |
|------|-----------|------|
| 对象 | `objectId` | GUID 字符串（与引擎一致；**不**强制 `guid://`） |
| 属性 | `propertyPath` | 与反射/ED-F12 基线一致 |
| 会话 | `sessionId` | Document Session 稳定 id |
| 名称 | `name` | **仅**展示/搜索结果，不作写目标 |

### 3.4 Scene MVP op 目录（草案）

| kind | op | 映射意图 |
|------|-----|----------|
| query | `editor.ping` | 连通 / 版本（连接内部；非 Agent Tool） |
| query | `editor.get_capabilities` | 能力清单真源出口 |
| query | `editor.get_state` | project、sessions 摘要 |
| query | `scene.get_hierarchy` | 树：id/name/children |
| query | `scene.get_object` | GO + components 摘要 |
| query | `scene.get_property` | Guid + path → 结构化值 |
| edit | `scene.create_game_object` | → AddEmpty GO Command |
| edit | `scene.delete_game_object` | → Delete GO Command |
| edit | `scene.add_component` | → AddComponent |
| edit | `scene.remove_component` | → RemoveComponent |
| edit | `scene.set_property` | → SetObjectProperty（Agent 默认 set） |
| edit | `scene.reparent` | → Reparent |
| edit | `scene.save` | Save（通常 non-undo） |
| verify | `scene.validate` | **P0 stub（诚实）：** 仅检查 session 有效 +（可选）objectId/path 可解析；**不**声称完整项目校验。描述须写明 stub，避免 Agent 误信 |

**映射规则：** 经 `IEditorAutomationSurface` → 域 `IScene*Service`；Edit 内才 `new EditorCommand`。细 op 可多于 MCP 粗 Tools。

### 3.4.0 `editor.get_state` 最小 result（冷启动 sessionId）

```json
{
  "projectPath": "D:/path/to/MyMEProject.meproject",
  "ready": true,
  "sessions": [
    {
      "sessionId": "sess-…",
      "documentType": "Scene",
      "assetId": "…",
      "title": "test.mescene",
      "dirty": false
    }
  ],
  "defaultSessionId": "sess-…"
}
```

| 字段 | 规则 |
|------|------|
| `sessions` | 当前已打开文档会话；可空数组 |
| `defaultSessionId` | 若 CLI `--open-scene` 成功打开，**必须**指向该 Session；否则可为 Active 信息性默认（Agent 写路径仍应显式传 sessionId） |
| `ready` | Host 已可接受自动化请求 |

Spawn 后 Agent：**先 `get_state`（或 connect 内建拉取）→ 取 `defaultSessionId` / `sessions[0]` → 再 edit**。

### 3.4.1 `editor.get_capabilities` 示例 result

```json
{
  "protocolVersion": 1,
  "editorProduct": "Maximum",
  "editorVersion": "0.0.9",
  "domains": [
    {
      "name": "editor",
      "actions": [
        {
          "name": "get_state",
          "kind": "query",
          "op": "editor.get_state",
          "sessionRequired": false,
          "description": "Project path and open sessions"
        },
        {
          "name": "get_capabilities",
          "kind": "query",
          "op": "editor.get_capabilities",
          "sessionRequired": false
        }
      ]
    },
    {
      "name": "scene",
      "actions": [
        {
          "name": "get_hierarchy",
          "kind": "query",
          "op": "scene.get_hierarchy",
          "sessionRequired": true
        },
        {
          "name": "get_property",
          "kind": "query",
          "op": "scene.get_property",
          "sessionRequired": true
        },
        {
          "name": "set_property",
          "kind": "edit",
          "op": "scene.set_property",
          "sessionRequired": true,
          "undo": true
        },
        {
          "name": "save",
          "kind": "edit",
          "op": "scene.save",
          "sessionRequired": true,
          "undo": false
        },
        {
          "name": "validate",
          "kind": "verify",
          "op": "scene.validate",
          "sessionRequired": true
        }
      ]
    }
  ]
}
```

### 3.4.2 常用 payload 形状（Scene MVP）

| op | payload 关键字段 |
|----|------------------|
| `scene.get_hierarchy` | （可空）`maxDepth?` |
| `scene.get_object` | `objectId` |
| `scene.get_property` | `objectId`, `propertyPath` |
| `scene.set_property` | `objectId`, `propertyPath`, `value` |
| `scene.create_game_object` | `name?`, `parentObjectId?` |
| `scene.delete_game_object` | `objectId` |
| `scene.add_component` | `objectId`, `className` |
| `scene.remove_component` | `componentObjectId`（组件 Guid；**不要**用模糊的 `objectId` 表示组件） |
| `scene.reparent` | `objectId`（子 GO）, `newParentObjectId` |
| `scene.save` | （可空） |
| `scene.validate` | （可空）；P0 stub 见上 |

值 `value`：JSON 原生类型 / 数组 / 对象。**实现前 spike：** 对齐现有反射/JSON 序列化路径（禁止另发明一套 Transform 编码）；约定写入本 Feature Implementation / Progress。

### 3.5 传输与帧定界（已拍板）

- 载体：Windows Named Pipe。  
- 帧：**4-byte little-endian length + UTF-8 JSON body**（请求与响应均如此）。  
- 正文形状：§3.1–3.2 信封。  
- 与 MCP 解耦：换 TCP 时只换载体，不改 JSON。  
- **RPC 超时：** 客户端（MCP）默认建议 **30s**（可配置）；超时 → `Timeout`，不假设写已提交。

### 3.6 仓库落点（建议）

```text
schemas/automation/v1/
  request.schema.json
  response.schema.json
  examples/
```

TS（MCP-F03）与 C++（ED-F17）共享 Schema，避免两处手写漂移。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 自定义 JSON + version | 简单、可读 | 需自管演进 | **选用 MVP** |
| B. JSON-RPC 2.0 | 现成 | 与三分 kind 略别扭 | 可后兼容封装 |
| C. Protobuf/gRPC | 强类型 | 过重 | 否决 MVP |
| D. 裸 JSONL（无长度前缀） | 易手测 | 粘包/部分读 | **否决**（已选 length-prefix） |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| op 与 Command 类名 1:1 绑死 | 重构痛 | op 稳定；映射经域服务 |
| 大 hierarchy 一次返回 | 卡顿 | 后置分页；MVP 可限深度/数量并警告 |
| Save 语义不清 | 丢数据 | 明确 dirty / 路径 / 失败码 |

---

## 6) 验收标准

- [x] `protocolVersion=1` 信封与 Scene 细 op 表评审通过
- [x] length-prefix 帧约定写入 `schemas/automation/v1/README.md`（examples 为 JSON 正文；帧在传输层）
- [x] Scene MVP op / payload 表已定稿（域服务 / EditorCommand 类映射属 ED-F17）
- [x] Schema 落盘：`schemas/automation/v1/`（request/response + examples + ERROR_CODES）

---

## 7) 决策与仍可议

**已拍板：** GUID 无强制 `guid://`；length-prefix；kind=`query|edit|verify`；**op 主键 + kind 一致性校验**；`get_state` 最小形状；`componentObjectId`；validate P0 stub 诚实声明；RPC 默认 30s。

**仍可议：**

1. `scene.save` 是否允许不经 Session（直接路径）？  
2. `edit` 政策（set vs edit specifier）MVP 是否只暴露 set？  
3. 内部 `editor.ping` 协议保留（连接用）——可以；不进 Agent Tool。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-17 | Draft：信封、Scene op、映射规则、开放点 |
| 2026-09-17 | 对齐 Q/E/V；细 op vs 粗 Tool |
| 2026-09-17 | length-prefix；get_capabilities；Surface 命名 |
| 2026-09-17 | capabilities 示例 + payload 表 |
| 2026-09-17 | **评审修补：** op/kind 规则、get_state、componentObjectId、validate stub、错误分类、超时；待审批 |
| 2026-09-17 | 契约落地 Done：`schemas/automation/v1/`；Impl S01–S03 收口 |
