# BUG-EDITOR-006 — AutomationHost::Stop 在 MCP pipe 仍连接时挂死 Shutdown

## Meta
- **ID:** BUG-EDITOR-006
- **Status:** Fixed
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-10-08
- **Last updated:** 2026-10-08
- **Affects:** `AutomationHost` / `AutomationPipeServer`；MCP named pipe 仍连接时 File→Exit
- **Related Feature/Slice:** ED-F17 Automation Host；minCraft feedback `ENGINE_FEEDBACK_PROMPT_AUTOMATION_STOP_HANG.md`

## TL;DR
同步 `ConnectNamedPipe`/`ReadFile` 无法被 `CancelIoEx` 可靠打断；`Editor::Shutdown` 堵在 `m_IoThread.join()`。已改为 overlapped IO + stop event，Stop 先 `InterruptIo` 再 join。

---

## 症状
MCP client 保持 pipe 连接时关 Editor → 进程 `Responding=False`，数十秒不退出。无 MCP 时退出正常。

## 期望
客户端仍连接时 File→Exit，进程数秒内结束。

## 复现
1. MCP attach 后 hold（不 `client.close()`）
2. 窗口 X / File→Exit（勿 taskkill）
3. 进程挂起

## 环境
- Windows；Debug MaximumEditor；minCraft-gameplay 反哺 2026-10-08

## 根因
`CreateNamedPipeW` 无 `FILE_FLAG_OVERLAPPED`；IO 线程阻塞在同步 Connect/Read；`Stop` 的 CancelIoEx/CloseHandle 不能可靠唤醒 → `join` 永不返回。

## 修复
- Pipe：`FILE_FLAG_OVERLAPPED` + stop/io events；`WaitForOverlappedCompletion`
- `InterruptIo`（SetEvent + CancelIoEx + Disconnect，不立刻 CloseHandle）→ join → `Close`
- Stop：2s `WaitForSingleObject` 诊断超时后重试 Interrupt 再 join
- 回归：`automation-pipe-interrupt` InterruptIo 打断 WaitForClient

## 回归验证
- [x] `minEngineTests` suite `automation-pipe-interrupt`
- [ ] 手动：MCP hold pipe → File→Exit 数秒内退出

## 关联
- [ED-F17](../Editor/ED-F17_AUTOMATION_HOST_DESIGN.md)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-08 | 记录 + 重叠 IO 修复 |
