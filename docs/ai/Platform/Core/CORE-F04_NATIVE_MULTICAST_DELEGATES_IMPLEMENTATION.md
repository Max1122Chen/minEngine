# Native Multicast Delegates — Implementation Plan

## Meta
- **ID:** `CORE-F04`
- **Type:** Implementation
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-04
- **Related:** [Design](./CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)

## TL;DR

按 Design 落地 `Runtime/Core/Delegates/` Native multicast；S01 类型+测试，S02 MEObject 弱绑定，S03 文档/债收口。不含 PHYS-F03。

## Reader quick start
1. [Design](./CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)
2. 本文件切片与验收。
3. 代码：`Runtime/Core/Delegates/`；`minEngineTests.exe test delegates`

---

## Slices

### S00 — Design 定稿
- **DoD:** [x] 用户确认 Design §7

### S01 — Multicast + Handle + 测试骨架
- **DoD:**
  - [x] 头文件在 `Runtime/Core/Delegates/`
  - [x] 工程 GLOB / 测试注册
  - [x] 多监听 / Remove(handle) / Broadcast 中 Remove

### S02 — MEObject 弱绑定
- **DoD:**
  - [x] 销毁后 Broadcast 跳过 + 惰性剔除（测试钉住）
  - [x] Raw 仍可用

### S03 — 文档与债收口
- **DoD:**
  - [x] TD-006 Done（Native）；Dynamic/Lua 另议
  - [x] 不强制迁移 AssetManager::Subscribe

---

## 非本计划

- PHYS-F03 Contact 派发实现
- Dynamic / Lua 事件
- AssetManager Subscribe 迁移（可选另开 chore）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-04 | 初稿：S00–S03 |
| 2026-08-04 | S01–S03 完成；`test delegates` 5/5 PASSED |
