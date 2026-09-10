# GameplayEventSystem — Implementation Plan

## Meta
- **ID:** `GP-F02`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Related:** [Design Spec](./GP-F02_GAMEPLAY_EVENT_SYSTEM_DESIGN.md), [GP-F01](./GP-F01_GAMEPLAY_TAG_DESIGN.md)

## TL;DR

S01–S04 Done；S05 结论：依赖 Scene 克隆复制带 `GameplayEventSystemComponent` 的 GO 即可，本期未改 Duplicator。`test gameplay-events` PASS。

## Scope
- **In:** `Runtime/Function/GameplayFramework/Events/`、`Scene` 查找辅助、测试 suite
- **Out:** Payload、Lua、Editor、ASC、异步 dispatch

## Reader quick start
1. [Design](./GP-F02_GAMEPLAY_EVENT_SYSTEM_DESIGN.md)
2. 下表；**阻塞：** `GP-F01` S02+（Manager + Container）

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `GP-F02-S01` | `GameplayEvent` + `GameplayEventChannel` | Done | 编译 |
| `GP-F02-S02` | `GameplayEventSystemComponent` 核心 API | Done | 单 Scene 测试 |
| `GP-F02-S03` | `Scene::GetGameplayEventSystem` + 重复/缺失 Warn | Done | 查找测试 |
| `GP-F02-S04` | 过滤 / priority / depth + 双 Scene 隔离 | Done | `test gameplay-events` |
| `GP-F02-S05` | PIE 克隆路径核对 | Done | 结论：无 Duplicator 改动；克隆带组件即隔离 |

---

## 2) 切片详情

### GP-F02-S01 — Event + Channel 类型
- **Goal:** tags-only `GameplayEvent`；Channel 包装 `GameplayTag`；无 payload API。
- **Touch:** `GameplayFramework/Events/*`
- **DoD:**
  - [ ] 头文件不暴露未完成 payload
- [ ] 依赖 GP-F01 类型
- **Verify:** 编译。

### GP-F02-S02 — System Component
- **Goal:** `Dispatch` / `Subscribe` / `Unsubscribe`；default channel；同步调用。
- **Touch:** `GameplayEventSystemComponent.*`；反射登记（若 Component 需出现在编辑器，可先 Invisible/内部）。
- **DoD:**
  - [ ] 单通道多监听可测
- **Verify:** 基础用例。

### GP-F02-S03 — Scene 查找
- **Goal:** Scene 访问器；0/1/N 个 Component 的行为符合 Design。
- **Touch:** `Scene.h` / `Scene.cpp`（或等价）
- **DoD:**
  - [ ] 文档约定与实现一致
- **Verify:** 查找用例。

### GP-F02-S04 — 合同钉死
- **Goal:** `requiredAll`/`requiredAny`、priority、registration order、`maxDispatchDepth`；两 Scene 互不串扰。
- **Touch:** 测试为主；必要时修实现。
- **DoD:**
  - [ ] Design §6 主要项勾选
- **Verify:** `minEngineTests.exe test gameplay-events`。

### GP-F02-S05 — PIE 路径
- **Goal:** 确认 Play 克隆后 PIE Scene 能拿到独立总线；缺口则最小补齐（例如默认系统 GO / EnterPlay 确保组件）。
- **Touch:** Play Mode / Duplicator 相关（仅必要时）
- **DoD:**
  - [ ] 记录结论于 Progress；若 Defer 写清 Unblock
- **Verify:** 手动 PIE 或自动化 clone 夹具。

---

## 3) 依赖顺序

```text
GP-F01-S02/S03 → GP-F02-S01 → S02 → S03 → S04 → S05
```

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock condition | Next check |
|----------|--------|-------------------|------------|
| Payload | 正式应用尚远 | 首个真实消费者 | 另切片 |
| Lua subscribe | 非基底 | 脚本玩法需要 | 另 Feature |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Draft 切片 S01–S05 |
