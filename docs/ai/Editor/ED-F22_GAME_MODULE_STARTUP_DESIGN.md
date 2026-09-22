# Game Module Startup Hook — Design Spec

## Meta
- **ID:** `ED-F22`
- **Type:** Feature
- **Status:** Cancelled
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - [Implementation](./ED-F22_GAME_MODULE_STARTUP_IMPLEMENTATION.md)（不实施）
  - [ED-F20](./ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md)（原 S03；保持不实现）
  - [CORE-F27](../Platform/Core/CORE-F27_EXTERNAL_COMPONENT_ATTACH_DESIGN.md)
  - [ED-F21](./ED-F21_GAME_MODULE_LINK_ENGINE_CONTRACT_DESIGN.md)
  - 发现场景（只读）：minCraft `ENG-GAP-006-C`（缺口关闭方式：场景数据驱动，非 C 钩子）
- **Branch:** `master`

## TL;DR

**Cancelled。** 引擎不提供 `MinEngine_GameModuleStartup` 作为官方游戏入口。世界搭建（CreateScene / Add GO / Add Component）属**游戏仓 / 工程资产策略**；应通过默认场景、手挂或编辑器创建完成。强制 Initialize 末尾 C 钩子易沦为塞业务启动逻辑，且与「Capabilities, not Opinions」冲突。反射注册由 CORE-F27 静态 `.gen.cpp` 在 Finalize 前完成，不需要 Startup。

## Scope
- **In:** （无 — 不实施）
- **Out:** 全部原计划；LoadLibrary 热插拔仍另议

## Reader quick start
1. 本文件 Status note — 为何取消
2. [CORE-F27](../Platform/Core/CORE-F27_EXTERNAL_COMPONENT_ATTACH_DESIGN.md) — 类型如何进表
3. [ED-F20](./ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md) — 仅链接宿主

---

## 7) Status note

| Field | Content |
|-------|---------|
| **Reason** | 维护者拍板：游戏应自建 Scene/GO/Comp；Startup 钩子即使存在也不该承载业务；ENG-GAP-006-C 用场景数据驱动即可，无需引擎强制 C 入口 |
| **Impact** | 不实现符号；ED-F20-S03 保持不做；外仓用默认场景 / 场景内 Component 完成 Dev 初始化 |
| **Options considered** | (A) 仍做空钩子禁业务 — 仍诱导误用；(B) Cancel — **选用** |
| **Decision** | **Cancelled**；ID 不回收 |
| **Unblock condition** | 仅当出现**非场景、非玩法**的真实机制缺口（例如多模块非反射注册面）且场景路径证伪时再开新 Feature（新 ID） |
| **Next check** | — |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft：升温 ED-F20-S03 |
| 2026-09-22 | 与 CORE-F27 划清：Startup 不做 RegisterClass |
| 2026-09-22 | **Cancelled**：世界搭建归游戏仓；不做官方 Startup 钩子 |

---

## 1) 背景与目标

### Pain
- 静态构造 / DllMain 时机早于引擎子系统就绪。
- 现靠场景手挂 Host Component → Dev 体验差。

### Success
```text
MaximumEditor 启动（已链游戏 DLL）
  → Engine/Editor Initialize …
  → MinEngine_GameModuleStartup() 恰好一次
  → 游戏可 Register / AddComponentInstance / 绑系统
```

---

## 2) 现状

| 项 | 状态 |
|----|------|
| ED-F20 S01–S02 | Done；`MINENGINE_HAS_GAME_MODULE` 可能已有（KeepAlive） |
| ED-F20-S03 | Deferred — 本 Feature 吸收 |
| GameModuleLinkKeepAlive | 强引用 Marker，防裁掉 |

---

## 3) 方案

### 3.1 符号契约

```cpp
// 游戏 SHARED 提供；C 链接，避免 mangling
extern "C" void MinEngine_GameModuleStartup(void);
```

可选（本期不做除非需要对称）：

```cpp
extern "C" void MinEngine_GameModuleShutdown(void);
```

### 3.2 调用点

- **Editor：** `Application::Initialize`（或等价「引擎已就绪、即将进主循环」）末尾，`#if MINENGINE_HAS_GAME_MODULE` 调用。
- **声明：** Editor 侧 `extern "C" void MinEngine_GameModuleStartup(void);`，由链接的游戏 DLL 解析；未启用游戏模块时**不编译**调用（与 KeepAlive 同宏）。
- **次数：** 进程生命周期一次（Initialize）；不每帧调用。

### 3.3 Stub

```cpp
static int g_StartupCalls = 0;
extern "C" void MinEngine_GameModuleStartup(void) { ++g_StartupCalls; }
// 另导出 getter 或复用现有测试通道断言 == 1
```

验证：开 stub 启 Editor `--version` 或最小自动化；断言调用计数（若 `--version` 走完整 Initialize）。

### 3.4 与 CORE-F27

```cpp
// 伪代码 — 游戏侧（类型已由 .gen.cpp 在 Finalize 前注册）
void MinEngine_GameModuleStartup(void) {
  // OK: 建默认 Host、AddComponent<MyPrim>()、绑系统
  // NOT OK: 首次 RegisterClass / 指望这里补 ME_CLASS
}
```

本期 stub **不强制**挂 Primitive；证明调用即可。Host 样例可与 CORE-F27-S03 联动。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Initialize 末尾显式 C 符号 | 简单、可文档化 | 非热插拔 | **选用** |
| B. 仅依赖静态构造 | 零引擎改动 | 时机不可靠 | **已证不够** |
| C. LoadLibrary + GetProcAddress | 热更 | 越界 | **Out** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| `--version` 不跑 Initialize | stub 难验 | 选确认会 Initialize 的入口，或加最小测试宿主 |
| 游戏未导出符号 | 链接失败 | 文档：启用 GAME 时必须提供；stub 作模板 |
| 调用过早/过晚 | 注册失败 | 锚在 Initialize 末尾；Design 写清前置（ObjectManager 等已有） |

---

## 6) 验收标准

- [ ] 符号与宏文档化
- [ ] 启用游戏模块时 Initialize 调用一次
- [ ] stub 实现并有可重复验证
- [ ] 默认无游戏模块：零回归
- [ ] 不实现 LoadLibrary

---

## 7) Status note

| Field | Content |
|-------|---------|
| **Reason** | 维护者拍板：游戏应自建 Scene/GO/Comp；Startup 钩子即使存在也不该承载业务；ENG-GAP-006-C 用场景数据驱动即可 |
| **Impact** | 不实现符号；ED-F20-S03 不做；外仓用默认场景 / 场景内 Component 完成 Dev 初始化 |
| **Options considered** | (A) 空钩子禁业务 — 仍诱导误用；(B) Cancel — **选用** |
| **Decision** | **Cancelled**；ID 不回收 |
| **Unblock condition** | 仅当出现非场景、非玩法的真实机制缺口且场景路径证伪时再开**新** Feature |
| **Next check** | — |

> 下文 §1–6 为取消前 Draft 正文，**仅作历史**；勿按验收实施。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft：升温 ED-F20-S03 → ED-F22；对应 ENG-GAP-006-C |
| 2026-09-22 | 与 CORE-F27 划清：Startup 不做 RegisterClass |
| 2026-09-22 | **Cancelled**：世界搭建归游戏仓；不做官方 Startup 钩子 |
