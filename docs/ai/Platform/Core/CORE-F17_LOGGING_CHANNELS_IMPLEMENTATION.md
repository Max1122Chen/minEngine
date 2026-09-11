# CORE-F17 — LogChannel + Structured LogRecord — Implementation Plan

## Meta
- **ID:** `CORE-F17`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-11
- **Related:** [Design Spec](./CORE-F17_LOGGING_CHANNELS_DESIGN.md)

## TL;DR

S00：typed Channel + LogRecord（含 Channel*）+ Sink + **全仓硬切** + Console 最小可读 Record。  
Fatal = Flush + abort。Editor 过滤产品化见 **ED-F09**。

## Scope
- **In:** Design §In  
- **Out:** LogField 真实现；旧宏兼容层；Console 高级 UX（ED-F09）

## Reader quick start
1. Design §5 迁移规则、§10 拍板  
2. 下表  

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| CORE-F17-S00 | 核心模型 + 硬切宏 + Console=LogRecord（最小） | **Done** | Engine/Editor/Tests 构建 |
| CORE-F17-S01 | 单测 threshold + Snapshot | **Done** | `minEngineTests.exe test logging-channels` |
| ED-F09 | Console Channel/Severity 过滤 UX | Planned | Editor 手测 |

---

## 2) 切片详情

### CORE-F17-S00 — Core cutover
- **Goal:** 新日志面成为唯一路径；Storage/Editor **最小**消费 `LogRecord`。  
- **Touch:** `Runtime/Core/Log/*`；全仓 `ME_*` 调用点；Editor ConsoleWindow 适配编译；删 `LogConsoleEntry`/`LogSource`/`Get*Logger`  
- **DoD:**
  - [x] DECLARE/DEFINE + 内置 Channels（显示名 A）  
  - [x] `ME_LOG` only；旧宏与 Get*Logger 删除  
  - [x] `LogRecord` 含 `channel*` + `channelName`  
  - [x] Storage 存 `LogRecord`；ConsoleWindow **最小可编译**  
  - [x] Fatal → Flush + abort  
  - [x] Field 仅占位  
- **Verify:** 全目标构建；Fatal 测试慎用（会 abort）  

### CORE-F17-S01 — Tests
- **Goal:** 低于 threshold 不出现；Snapshot 元素为 LogRecord。  
- **Verify:** doctest suite  

### ED-F09 — Console filter UX（另 Feature）
- **Goal:** 按 channel / severity 过滤与展示打磨。见 [ED-F09 Design](../../Editor/ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md)。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-11 | S00/S01 落地：硬切 ME_LOG；Fatal=abort；logging-channels 单测通过 |
| 2026-09-11 | Fatal=abort；Record Channel*；Console UX → ED-F09 |
| 2026-09-11 | 初稿对齐拍板：硬切、无 Entry、Field 暂缓 |
