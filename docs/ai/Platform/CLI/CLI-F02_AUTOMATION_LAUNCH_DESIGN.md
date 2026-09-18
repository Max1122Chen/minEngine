# CLI-F02 — Automation 启动面（拉起 Editor + 握手）— Design Spec

## Meta
- **ID:** `CLI-F02`
- **Type:** Feature
- **Status:** Done（启动面：flags + open-scene + launch 配置缓存；ready-file/Listen → ED-F17）
- **Last updated:** 2026-09-18
- **Branch:** `feat/mcp`
- **Depends on:** `CLI-F01` Done；配合 `ED-F17` / `MCP-F03`
- **Related:**
  - [Implementation](./CLI-F02_AUTOMATION_LAUNCH_IMPLEMENTATION.md)（**Done**）
  - [MCP-F01](../../Editor/MCP/MCP-F01_ARCHITECTURE_DESIGN.md)
  - [ApplicationCommandLine](../../../minEngine/minEngine/src/Runtime/Core/CLI/ApplicationCommandLine.h)

## TL;DR

扩展统一 CLI：支持 **Spawn**（拉起 Maximum + pipe + 可选 open-scene + ready）以及 **Attach** 前提（人类已开的 Editor 也用同一套 flag 启用 Host）。不发明第二套 argv 解析器。对接产品 **minEngine MCP Server**。

## Scope
- **In:** automation options、ready 约定、退出码、Spawn 时序；Attach 时「仅开 Host」的同一 flags
- **Out:** MCP Server 本身、协议 op、完整无头 Automation.exe

## Reader quick start
1. §3.1 建议 flags
2. §3.2 时序
3. §3.3 退出码

---

## 1) 背景与目标

维护者意图：Agent **本身就可以 CLI 拉起编辑器**。因此 MVP 默认是 **Spawn**，不是「只能附着已开 GUI」。

本 Feature 回答的不是「要不要拉起」，而是：

1. 拉起时如何声明 automation 端点？  
2. 何时算 ready？  
3. 如何打开正确的 Scene Session，避免 Agent 改错文档？

---

## 2) 现状

`ApplicationCommandLine`（CLI-F01）：

- 默认 Editor 模式；`--project/-p`；`test` 子命令。
- **无** automation pipe / ready / 自动打开 scene 的正式约定。

---

## 3) 方案

### 3.1 建议 flags（草案，Editor 模式）

| Flag | 含义 |
|------|------|
| `--project <path>` | 已有；必填（自动化场景） |
| `--automation-pipe <name>` | 启用 ED-F17 Host；**建议含 pid/随机**（勿固定全局名） |
| `--open-scene <asset-path-or-guid>` | 启动后打开指定 Scene；成功后 `get_state.defaultSessionId` 须指向它 |
| `--automation-ready-file <path>` | **主 ready 通道**；仅在 Listen（及 open-scene）成功后写入 |
| （可选）stdout | 单行 `AUTOMATION_READY pipe=…`（日志吵则可不依赖） |

### 3.2 时序（Spawn）

**不变量：** `Listen(pipe) → open-scene（若有）→ 写 ready-file`。

```text
Agent / minEngine MCP Server
  │ spawn Maximum.exe --project P --automation-pipe N --open-scene S
  │        --automation-ready-file R
  ▼
Maximum → 加载项目 → 打开 Scene Session → Host Listen(N)
  ▼
写 ready-file R（+ 可选 stdout）   // 不得早于 Listen
  ▼
MCP 看到 R → connect(N) → 内部连通检查 → get_state → …
```

**Attach（MVP）：** 人类启动时同样带 `--automation-pipe`（及项目）；MCP **只 connect**。未开 automation → 失败 + 提示如何加 flag。

### 3.3 退出码（草案）

沿用 CLI-F01：`0` 成功，`1` 运行失败，`2` 用法错误。  
`--automation-pipe` 已给但 Host 初始化失败 → `1`。

### 3.4 与 MCP-F03 的分工

| 角色 | 职责 |
|------|------|
| minEngine MCP Server | spawn/attach、等 ready-file、超时、Tool 翻译 |
| CLI-F02 | 解析 flags、交给 Editor/Host |
| ED-F17 | listen、写 ready-file、执行 Q/E/V |

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| 仅环境变量、无 CLI flag | 否决为唯一方式 |
| 固定全局 pipe 名 | 否决；用参数/pid |
| Ready 仅靠 Agent Tool ping | 否决为 Spawn 主通道（ready-file 更简单）；ping 可作内部检查 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| 未加载完就接请求 | ready-file 门闸 |
| 打开错 Scene | `--open-scene` + 返回 sessionId |
| stdout 被日志淹没 | **ready-file 为主** |

---

## 6) 验收标准

- [x] `--help` 可见 automation options
- [ ] 带 pipe 启动后，超时内出现 ready-file — **ED-F17**（本 Feature 只缓存路径；Listen→写文件不变量属 Host）
- [ ] `--open-scene` 后 `editor.get_state` 可见对应 Session — **路径侧已打开场景**；`get_state` 协议出口属 **ED-F17**
- [x] 无第二套 argv 扫描

---

## 7) 仍可议

1. `--open-scene`：路径 vs Guid vs 两者？（MVP：**项目相对资产路径**）  
2. pipe 默认名模板？  

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-17 | Draft：Spawn、flags、时序 |
| 2026-09-17 | MVP Spawn+Attach；对接 minEngine MCP Server |
| 2026-09-17 | ready-file 为主；Attach 失败可读提示 |
| 2026-09-17 | **评审修补：** Listen→ready 不变量；pipe 含 pid/随机；open-scene↔defaultSessionId；待审批 |
| 2026-09-18 | 实现 Done（启动面）；ready-file / get_state 验收转 ED-F17 |
