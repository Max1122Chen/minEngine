# MCP-F02 — Automation Protocol — Implementation Plan

## Meta
- **ID:** `MCP-F02`
- **Status:** Done
- **Last updated:** 2026-09-17
- **Related:** [Design Spec](./MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md) · [MCP-F01](./MCP-F01_ARCHITECTURE_DESIGN.md) · [ED-F17](../ED-F17_AUTOMATION_HOST_DESIGN.md) · Schema: [`schemas/automation/v1/`](../../../../schemas/automation/v1/)

## TL;DR

本 Feature **只落协议契约**（JSON Schema + examples + 错误码表），不实现 Named Pipe / Host / MCP Server。三切片全部 **Done**；落盘路径：`schemas/automation/v1/`。

## Scope
- **In:** `schemas/automation/v1/`；request/response schema；examples；`ERROR_CODES.md`；Design 交叉链接；Registry/Progress
- **Out:** C++ Host、TS MCP Server、CLI flags、域服务实现（属 CLI-F02 / ED-F17 / MCP-F03）

## Reader quick start
1. [Design](./MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md) — 信封、op 表、`op` 主键
2. [`schemas/automation/v1/README.md`](../../../../schemas/automation/v1/README.md)
3. 验收后 ED-F17 / MCP-F03 读同一份 schema

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `MCP-F02-S01` | 目录 + `request`/`response` JSON Schema + 帧说明 README | **Done** | 对照 Design §3.1–3.2；`jsonschema` Draft 2020-12 校验 examples |
| `MCP-F02-S02` | examples：ping、get_state、get_capabilities、set_property ok/fail、KindOpMismatch、remove_component | **Done** | 字段与 Design §3.4 一致 |
| `MCP-F02-S03` | Design/Registry/Progress 回链；`ERROR_CODES.md` | **Done** | 链接可点；ACTIVE_WORK → CLI-F02 / ED-F17 |

状态：`Planned | In Progress | Done | Blocked | Deferred | Cancelled`

## 2) 切片详情

### MCP-F02-S01 — Schema 骨架与帧约定

- **Goal:** 建立共享协议落盘点，TS/C++ 日后同读，避免两处手写漂移。
- **Touch:**
  - `schemas/automation/v1/README.md`（帧：u32 LE length + UTF-8 JSON；`protocolVersion=1`）
  - `schemas/automation/v1/request.schema.json`
  - `schemas/automation/v1/response.schema.json`
- **DoD:**
  - [x] README 写明帧定界与 Design 链接
  - [x] request 含 `protocolVersion`/`requestId`/`kind`/`op`/`sessionId?`/`payload`
  - [x] response 含 `ok`/`result?`/`errors`/`warnings?`
  - [x] `kind` enum = `query|edit|verify`
- **Verify:**
  - 对照 Design §3.1、§3.2、§3.1.1（op 主键说明写在 README）
  - `python` + `jsonschema` Draft202012Validator：全部 `examples/*.json` PASS（见 Progress）

### MCP-F02-S02 — Examples（Scene / editor）

- **Goal:** 可给 ED-F17 最小客户端与 MCP-F03 当金色样例。
- **Touch:** `schemas/automation/v1/examples/`
  - `editor_ping.request.json` / `.response.json`
  - `editor_get_state.request.json` / `.response.json`（含 `sessions`/`defaultSessionId`）
  - `editor_get_capabilities.request.json` / `.response.json`（Scene MVP 子集）
  - `scene_set_property.request.json` / `.response.ok.json` / `.response.fail_invalid_object.json`
  - `kind_op_mismatch.response.json`
  - `scene_remove_component.request.json`（`componentObjectId`）
- **DoD:**
  - [x] get_state 形状符合 Design §3.4.0
  - [x] set_property payload：`objectId`/`propertyPath`/`value`
  - [x] fail 例使用 `InvalidObjectId` / `KindOpMismatch`
  - [x] `remove_component` 用 `componentObjectId`
- **Verify:** Design diff + schema 校验全部 examples

### MCP-F02-S03 — 文档收口

- **Goal:** 协议 Feature 可标 Done（契约层）；下游可开工。
- **Touch:**
  - Design Meta Related → Implementation；验收勾选
  - `FEATURE_REGISTRY` Status → **Done**；Impl 链接
  - `ACTIVE_WORK` / `PROGRESS_LOG`
  - `schemas/automation/v1/ERROR_CODES.md`
- **DoD:**
  - [x] ERROR_CODES.md 存在
  - [x] Registry / Progress / ACTIVE_WORK 更新
  - [x] Design 验收与本 Impl 对齐
- **Verify:** 打开链接无死链；下一刀 CLI-F02 或 ED-F17 Impl

## 3) 依赖顺序

```text
MCP-F02-S01 → MCP-F02-S02 → MCP-F02-S03   (Done)
         ↓
   ED-F17 / MCP-F03（读 schema）
   CLI-F02（可并行写 Impl；编码建议 schema 已就绪）
```

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock condition | Next check |
|----------|--------|-------------------|------------|
| （无）JSON Schema 生成 C++ 头 | 过早 | ED-F17 需要时再开 | P0 Host 后 |
| inputSchema 进 capabilities 全量 | P0 可省略 | Agent 发现性不够时 | MCP-F03 手验后 |

## 5) 风险

| 风险 | 缓解 |
|------|------|
| Schema 过严导致 Host 先写松 JSON 对不上 | P0 schema 对 `payload`/`result` 宽松；op 专用形状用 examples 钉死 |
| 仓库根 `schemas/` 与现有布局冲突 | **已采用** Design 推荐路径 `schemas/automation/v1/` |

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-17 | 初稿；待审批后落盘 schema |
| 2026-09-17 | S01–S03 落地；Feature **Done**（契约层）；等待 Feature 审批后开下游 Impl |
