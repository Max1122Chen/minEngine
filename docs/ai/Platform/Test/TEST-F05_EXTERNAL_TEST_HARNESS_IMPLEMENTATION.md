# TEST-F05 — Implementation Plan

## Meta
- **ID:** `TEST-F05`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-21
- **Related:** [Design Spec](./TEST-F05_EXTERNAL_TEST_HARNESS_DESIGN.md)

## TL;DR

S01–S02 **Done**。路径 B（S03）仍 Deferred。

## Scope
- **In:** S01–S02
- **Out:** 测试 DLL 插件；强制外仓进 CI

## Reader quick start
1. Design
2. 下表
3. Progress

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `TEST-F05-S01` | CMake EXTRA_*；`RegisterExtraTestSuites` 钩子；重复 Id 检测 | **Done** | 默认 smoke |
| `TEST-F05-S02` | Stub `game.stub-extra` + 文档（BOOTSTRAP / Fixtures README） | **Done** | `test game.stub-extra` |
| `TEST-F05-S03` | 路径 B：`MINENGINE_API` 导出 Runner/Registry | Deferred | 游戏仓自建 exe 链通 |

## 2) 切片详情

### TEST-F05-S01 — 扩展点
- **Goal:** 可开关的外接编译与注册。
- **Touch:** `Tests/CMakeLists.txt`、`TestSuiteRegistration.cpp`、`ExtraTestRegistration.h`、`TestSuiteRegistry::Register` 重复检查。
- **DoD:**
  - [x] 无 EXTRA 时行为不变
  - [x] 有 EXTRA 时编译定义 `MINENGINE_ENABLE_EXTRA_TESTS` 并调用 `RegisterExtraTestSuites`
- **Verify:** `.\scripts\verify.ps1` PASS

### TEST-F05-S02 — Stub + 文档
- **Goal:** 引擎自证；外仓可抄作业。
- **Touch:** `Tests/Fixtures/ExtraTestStub/`；`BOOTSTRAP_DIGEST`
- **DoD:**
  - [x] stub Id=`game.stub-extra`，`InSmoke=false`
  - [x] 文档含变量表、注册函数签名、doctest IMPLEMENT 禁令
- **Verify:** `minEngineTests.exe test game.stub-extra` PASS

### TEST-F05-S03 — 路径 B（Deferred）
- **Goal:** 外仓独立测试 exe 稳链同一 Runner。
- **Unblock:** 路径 A 不够用或外仓拒绝进引擎 configure。
- **Next check:** 第一个外仓 CI 需要独立 exe 时。

## 3) 依赖顺序

```text
S01 → S02
S03 可选
```

可与 `ED-F20` 并行；若同一 configure 既链游戏又挂测，建议 **先 ED-F20-S01** 再测 link 游戏目标进 EXTRA。

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock | Next check |
|----------|--------|---------|------------|
| S03 | MVP 用 A 即可 | 独立 exe 刚需 | 外仓 CI 反馈 |
| C 插件式 | 过大 | 热加载测试需求 | 不排期 |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-21 | Draft 切片计划 |
| 2026-09-21 | S01–S02 Done |
