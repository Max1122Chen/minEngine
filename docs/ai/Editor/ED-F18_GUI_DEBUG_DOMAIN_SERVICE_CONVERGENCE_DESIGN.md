# ED-F18 — GUI / Debug 收敛到同源域服务 — Design Spec

## Meta
- **ID:** `ED-F18`
- **Type:** Feature（收敛 / 小重构）
- **Status:** Planned（挂账；**不**挡 MCP P0）
- **Owner:** project maintainer
- **Last updated:** 2026-09-17
- **Branch:** `feat/mcp`（实现可另开）
- **Depends on:** `ED-F17` 域服务（`ISceneEditService` 等）落地或可调用
- **Related:** [ED-F17](./ED-F17_AUTOMATION_HOST_DESIGN.md) · [MCP-F01 §3.4–3.5](./MCP/MCP-F01_ARCHITECTURE_DESIGN.md) · [ED-F12](./ED-F12_AGENT_EDIT_PROTOCOL_DESIGN.md) · [ED-F13](./ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md)
- **Blocks:** 无（质量债 / 真同源完整度）

## TL;DR

MCP P0 先让 **Automation Host** 走 L1 域服务。本 Feature 事后把 **GUI / DebugCommand** 仍「直接 `new EditorCommand`」的路径改成调用 **同一套** Query/Edit 服务，消除双轨。有空再做；不阻塞 minEngine MCP Server 竖切。

## Scope
- **In:** Scene（及后续已有域服务的类型）GUI 写路径、Debug 解析成功后的执行路径，改为调域服务；删掉重复直写（若有）
- **Out:** 新业务能力；MCP Server；新资产域；大范围 UX

## Reader quick start
1. MCP-F01 §3.5 同源意图
2. ED-F17 §3.1 包装边界
3. 本文件验收

---

## 1) 背景

ED-F17 引入域服务后，Host/MCP 已同源；若 GUI/Debug 仍绕过服务，长期会出现「Agent 能 Undo、按钮另一套语义」的漂移。

## 2) 方案（摘要）

```text
Before:  GUI/Debug ──new Command──► Stack
         Host     ──DomainService──► Command──► Stack

After:   GUI/Debug/Host ──DomainService──► Command──► Stack
```

按域分切片（先 Scene 属性/GO 主路径）。

## 3) 验收

- [ ] Scene 主写路径（至少 SetProperty + 一两个拓扑操作）GUI 与 Debug 均经域服务
- [ ] 与 Host 同路径的手验或测试说明写入 Progress
- [ ] 无新增 MCP 语义

## 4) Status note

| 字段 | 内容 |
|------|------|
| Reason | 挂账；优先 MCP P0 |
| Impact | 同源不完整，直到本 Feature Done |
| Options | 与 ED-F17 同 PR 硬收 / 本 Planned 后置 |
| Decision | **后置 Planned** |
| Unblock | ED-F17 域服务可用 + 维护者排期 |
| Next check | MCP Scene 闭环后 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-17 | 登记挂账；不挡 MCP P0 |
