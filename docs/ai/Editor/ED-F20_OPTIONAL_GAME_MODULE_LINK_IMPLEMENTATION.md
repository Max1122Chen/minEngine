# ED-F20 — Implementation Plan

## Meta
- **ID:** `ED-F20`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-21
- **Related:** [Design Spec](./ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md)

## TL;DR

S01–S02 **Done**。S03 Startup 符号仍 Deferred。

## Scope
- **In:** S01–S02
- **Out:** S03；LoadLibrary；minCraft 源码进树

## Reader quick start
1. Design — 契约
2. 下表 — 切片
3. `PROGRESS_LOG.md` — 落地记录

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `ED-F20-S01` | 根/`Editor` CMake 选项；条件 `add_subdirectory` + link；BOOTSTRAP 一小节 | **Done** | 默认编 Editor；文档有变量表 |
| `ED-F20-S02` | `Examples/GameModuleStub`；`COPY_RUNTIME`；开 stub 编通并启动；默认 smoke | **Done** | stub ON 编过；默认 `verify.ps1` |
| `ED-F20-S03` | `MinEngine_GameModuleStartup` 钩子 | Deferred | — |

## 2) 切片详情

### ED-F20-S01 — CMake 钩子 + 文档
- **Goal:** 可配置开关；关 = 今日行为。
- **Touch:** `minEngine/CMakeLists.txt`、`Editor/CMakeLists.txt`；`BOOTSTRAP_DIGEST.md`
- **DoD:**
  - [x] `MINENGINE_GAME_DIR` / `MINENGINE_GAME_TARGET` / `MINENGINE_GAME_COPY_RUNTIME` 存在且有注释
  - [x] 未设置时无 `add_subdirectory`、无额外 link
  - [x] 设置但不存在 TARGET → 清晰 `FATAL_ERROR`
- **Verify:** 默认 `cmake --build … --target Editor`

### ED-F20-S02 — Stub + 运行时拷贝
- **Goal:** 引擎自证；DLL 落在 Editor `bin`。
- **Touch:** `Examples/GameModuleStub/`；POST_BUILD copy；`GameModuleLinkKeepAlive.cpp`（仅 stub 强引用 marker）
- **DoD:**
  - [x] Stub SHARED 可被 Editor 链接
  - [x] 开开关后 `EXECUTABLE_PATH` 可见 stub DLL
  - [x] 关开关：`verify.ps1` smoke 通过
- **Verify:**
  - 默认：`.\scripts\verify.ps1` PASS
  - Stub：`-DMINENGINE_GAME_DIR=…/GameModuleStub -DMINENGINE_GAME_TARGET=GameModuleStub` → `MaximumEditor.exe --version` PASS

### ED-F20-S03 — Startup 符号（Deferred）
- **Goal:** Editor Initialize 后调用游戏导出的 C 符号。
- **Unblock:** 实践证明仅链接不够。
- **Next check:** 游戏仓反馈。

## 3) 依赖顺序

```text
S01 → S02
S03 独立，默认跳过
```

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock | Next check |
|----------|--------|---------|------------|
| S03 | MVP 仅链接足够 | 显式 init 刚需 | 游戏仓反馈 |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-21 | Draft 切片计划 |
| 2026-09-21 | S01–S02 Done |
