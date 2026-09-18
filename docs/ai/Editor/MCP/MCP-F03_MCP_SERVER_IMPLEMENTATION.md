# MCP-F03 — minEngine MCP Server — Implementation Plan

## Meta
- **ID:** `MCP-F03`
- **Status:** Done
- **Last updated:** 2026-09-18（S01–S03 落地；attach/spawn 冒烟 PASS）
- **Related:** [Design Spec](./MCP-F03_MCP_SERVER_DESIGN.md) · [MCP-F02](./MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md) · [ED-F17](../ED-F17_AUTOMATION_HOST_DESIGN.md) · [CLI-F02](../../Platform/CLI/CLI-F02_AUTOMATION_LAUNCH_DESIGN.md)

## TL;DR

Node/TS stdio MCP Server：粗 Tools + Pipe Client + Spawn/Attach。**无业务语义**；能力真源来自 Host `editor.get_capabilities`。三切片 **Done**。

## Scope
- **In:** `minEngineMcpServer/`；length-prefix Pipe；连接状态机；5 粗 Tools；错误码映射
- **Out:** C++ 改动；细 Tool 目录；业界 MCP 调研；GUI

## Reader quick start
1. Design §3.1–3.4
2. 下表切片
3. `schemas/automation/v1/README.md` 帧格式 · `minEngineMcpServer/README.md`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `MCP-F03-S01` | 包骨架 + protocol 类型 + Named Pipe client（帧读写） | **Done** | `npm run smoke:ping` → Host ping ok |
| `MCP-F03-S02` | Connection Manager：attach + spawn + wait ready-file；拉 capabilities/state | **Done** | `smoke_attach` / `smoke_spawn` PASS |
| `MCP-F03-S03` | 注册粗 Tools；description 声明能力来自 Host | **Done** | `npm run build`；Tools 已注册 |

## 2) 切片详情

### MCP-F03-S01 — 骨架 + Pipe Client

- **Goal:** 可独立测的传输与协议编解码。
- **Touch:** `minEngineMcpServer/` · `src/automation/*`
- **DoD:**
  - [x] `npm install` / `npm run build` PASS
  - [x] Pipe：u32 LE + UTF-8 JSON 读写
  - [x] `requestId` 生成；默认超时 30s
- **Verify:** 对运行中的 Maximum（`--automation-pipe`）发 `editor.ping`

### MCP-F03-S02 — 连接

- **Goal:** Spawn/Attach → Ready（capabilities + get_state）。
- **Touch:** `src/connection/manager.ts`
- **DoD:**
  - [x] attach(pipeName)
  - [x] spawn(editorPath, project, openScene?, pipe, readyFile)
  - [x] 连接后必拉 `editor.get_capabilities`（禁 TS 独真源）
- **Verify:** attach + spawn 各一次

### MCP-F03-S03 — 粗 Tools

- **Goal:** Agent 可调的 L3 外观。
- **Tools:** `editor_connect` · `editor_get_state` · `editor_query` · `editor_edit` · `editor_verify`
- **DoD:**
  - [x] Design §6 验收勾选
  - [x] `domain`+`action` → `kind`+`op` 字符串拼接；无 Transform 业务分支
- **Verify:** build + attach 路径下 `scene.get_hierarchy`；Cursor 手配见 README

## 3) 依赖顺序

```text
S01 → S02 → S03
```

## 4) 延后

| Item | Reason |
|------|--------|
| editor_disconnect 产品 Tool | MVP 可选；进程退出即断 |
| 细 Tool 目录 | Design 否决默认 |
| npm scope `@minengine/mcp-server` | 包名暂用 `minengine-mcp-server` |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | Impl 开工；依赖 ED-F17/CLI-F02/MCP-F02 已齐 |
| 2026-09-18 | S01–S03 Done；ping/attach/spawn 冒烟 PASS；**Status → Done** |
