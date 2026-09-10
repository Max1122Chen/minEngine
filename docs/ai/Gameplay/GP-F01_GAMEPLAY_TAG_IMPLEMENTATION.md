# GameplayTag — Implementation Plan

## Meta
- **ID:** `GP-F01`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Related:** [Design Spec](./GP-F01_GAMEPLAY_TAG_DESIGN.md)

## TL;DR

三个切片均已落地；`minEngineTests.exe test gameplay-tags` PASS。

## Scope
- **In:** `Runtime/Function/GameplayFramework/Tags/`、Engine 启停挂载、`gameplay-tags` 测试 suite
- **Out:** TagQuery、序列化进 Scene、Editor Picker、业务 Tag 表

## Reader quick start
1. [Design](./GP-F01_GAMEPLAY_TAG_DESIGN.md)
2. 下表切片
3. 依赖：无；被 [GP-F02](./GP-F02_GAMEPLAY_EVENT_SYSTEM_DESIGN.md) 依赖

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `GP-F01-S01` | 目录骨架 + `GameplayTag` + Native 宏头 | Done | 编译 |
| `GP-F01-S02` | `GameplayTagManager` + Engine 挂载 + 内置 `TAG_Channel_Default` | Done | `test gameplay-tags` |
| `GP-F01-S03` | `GameplayTagContainer` + 完整 suite（含宏注册） | Done | `minEngineTests.exe test gameplay-tags` |

---

## 2) 切片详情

### GP-F01-S01 — Tag handle + Native macros
- **Goal:** 创建 `Runtime/Function/GameplayFramework/Tags/`；`GameplayTag`；`ME_DECLARE/DEFINE_GAMEPLAY_TAG*` 与 `NativeGameplayTag`。
- **Touch:** `GameplayTag.*`、`NativeGameplayTags.h`（宏）、相关 `.cpp`；CMake 由 GLOB 纳入。
- **DoD:**
  - [ ] 路径与命名符合 Design
  - [ ] 宏可在头/源中按 UE 工作流使用
  - [ ] 无 ASC / 无 Event 代码混入
- **Verify:** 编译 `minEngine` 或相关 target。

### GP-F01-S02 — Manager + Engine
- **Goal:** 收集 Native 注册名建树（隐式父节点）；`Resolve`/`TryResolve`/`GetParent`/`IsValidTag`；Engine Initialize/Shutdown 拥有实例；`Get()` 可用；定义 `TAG_Channel_Default`。
- **Touch:** `GameplayTagManager.*`；`NativeGameplayTags.cpp`；`Engine.cpp` / `Engine.h`。
- **DoD:**
  - [ ] 启停无泄漏/无悬空 Get
  - [ ] 未知名 Resolve 失败策略实现并测
  - [ ] `TAG_Channel_Default` 可解析
- **Verify:** 至少 manager + macro 用例进 suite。

### GP-F01-S03 — Container + suite Done
- **Goal:** Container refcount、`Has`/`HasAll`/`HasAny`、`Clone`；测试钉住 UE 匹配方向。
- **Touch:** `GameplayTagContainer.*`；`Tests/.../GameplayTag*`。
- **DoD:**
  - [ ] Design §6 验收勾选
  - [ ] Progress 记一笔
- **Verify:** `minEngineTests.exe test gameplay-tags`（名称以实现为准）。

---

## 3) 依赖顺序

```text
S01 → S02 → S03 → (GP-F02)
```

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock condition | Next check |
|----------|--------|-------------------|------------|
| JSON 双源 | 正式应用尚远 | 项目需要数据驱动 Tag | 应用前 |
| TagQuery | MVP 不需要 | GA/GE 需求出现 | 另 Feature |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-04 | Draft 切片 S01–S03 |
