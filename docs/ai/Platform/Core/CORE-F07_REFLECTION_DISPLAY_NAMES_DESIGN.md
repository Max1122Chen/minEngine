# Reflection Display Names — Placeholder

## Meta
- **ID:** `CORE-F07`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-02
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md), [ACTIVE_WORK.md](../../ACTIVE_WORK.md)
- **Branch:** `feat/editor`（**非 master**；由 editor 轨交付）
- **Priority:** Low（与 ED-F02 并行）

## TL;DR
反射与 Inspector 展示：**去掉**成员名上的 `m_` / `x_` 等工程前缀（codegen 或运行时 display name），提升可读性；不改变 C++ 标识符。

## Status note（Planned）

| 字段 | 内容 |
|------|------|
| What's not | 正式 Design、codegen 规则、回归测试 |
| Branch | `feat/editor`（master 不排期） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | Registry 占位登记（双轨 backlog 审批稿） |
