# minEngine Design Philosophy

## Meta
- **ID:** N/A（跨 Feature 长期约束；非 Feature 排期源）
- **Status:** Active
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Related:** [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md), [ACTIVE_WORK.md](./ACTIVE_WORK.md), [PROJECT_CONTEXT.md](./PROJECT_CONTEXT.md)
- **Agent hook:** `.cursor/rules/engine-design-philosophy.mdc`（always apply）；bootstrap 读本文件摘要

## TL;DR

minEngine 提供**能力与机制**，不替游戏规定架构与业务语义。Core 保持克制；高层能力走 Plugin / Optional Module。系统可组合、可替换。Agent-friendly 是持续设计原则，不是附加 Framework。优先简单清晰的机制，不为模仿大型商业引擎而引入无收益复杂度。

## Scope
- **In:** 架构设计、Feature 规划、技术选型、Core vs Plugin 边界、Gameplay / Networking 方向约束
- **Out:** 具体 Feature 切片排期（见 Capability Roadmap / ACTIVE_WORK）

## Reader quick start
1. 读下方六条原则 + 九问检查表。
2. 新 Feature / 大设计前：对照原则做 Pre-flight；偏离时主动提醒维护者。
3. 排期与并行关系：见 [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md)。

---

## 1) Capabilities, not Opinions

Engine 提供足够强大的能力，但不替用户决定游戏应该如何构建。

**提供：** 通用能力、稳定抽象、基础机制、可组合系统、工具与自动化。

**不预先规定：** Gameplay 架构、游戏对象固定语义、业务逻辑组织方式、必须使用的完整 Framework。

> Engine provides capabilities, not opinions.

## 2) Mechanism over Policy

> Engine answers How; Game answers What and Why.

Engine 提供机制与抽象；具体项目决定语义与策略。

**可提供（示例）：** Ability / Effect / Attribute 类机制；Animation / State / Parameter；Navigation；Networking primitives；Asset / Serialization；Entity / Component；Rendering primitives。

**不应内置（示例）：** Enemy、Weapon、Inventory、Quest、角色专属 gameplay —— 除非本身已是足够通用的 Engine-level mechanism。

## 3) Minimal Core, Optional Extension

Core 保持克制。高层、具体领域、有明显设计倾向的能力，优先：

- Plugin
- Package
- Optional Module

尤其 **Gameplay Framework** 避免成为强制用户接受的完整游戏架构。

用户应能：只用 Core；按需启用模块；替换模块；完全绕过某些高层系统。

## 4) Composable, not Prescriptive

系统通过清晰接口组合，而非不可替换的巨大 Framework。

新系统设计优先问：

- 能否独立使用？
- 能否被其他系统组合？
- 能否被替换？
- 不使用该系统时 Engine 是否仍可用？
- 是否真的需要放入 Core？

不为“完整”而人为强耦合。

## 5) Agent-Friendly Design

不是未来再加的独立 Feature，而是持续原则。

重要能力尽可能具备：稳定结构化 API；可查询状态；可自动修改的数据；明确 Command / Operation；数据驱动与声明式接口；Reflection / Serialization；Editor / CLI / Lua / Agent 复用同一底层能力。

> 人类、Editor、脚本和 Agent 都调用同一套 Engine 能力。

新系统设计时主动考虑：Agent 如何发现 / 查询 / 修改 / 验证？

**不要**为 Agent 单独造复杂 Agent Framework；优先改善 Engine 本身的可观察性、可操作性与结构化程度。

## 6) Prefer Simplicity

目标不是复制 UE/Unity 的完整复杂度。这是个人 / 学习 Engine，也是验证 Engine Architecture 的长期项目。

> 若简单清晰的机制已能解决问题，不要为“现代引擎应该这样”而引入额外复杂度。

避免：过早抽象、过早泛化、为不存在的需求设计复杂 Framework、复制大型商业引擎的历史包袱。复杂度必须有实际收益。

---

## Decision checklist（重大 Feature 必问）

1. 这个能力是否真正通用？
2. 它应该属于 Core 还是 Plugin？
3. 是否在替用户做不必要的架构决定？
4. 是否可以通过更简单的机制解决？
5. 是否与现有 Engine mechanism 自然组合？
6. 是否增加了不必要的耦合？
7. 是否容易被 Editor / Script / Agent 操作？
8. 是否为未来的扩展留下合理空间？
9. 这个 Feature 是否真的值得**现在**做？

---

## Development mode（阶段约束）

当前阶段：**快速扩展整体能力、形成完整开发体验** —— 不是严格线性清空 TODO。

模式：**一条主航道 + 多条可并行支线**。不人为建立不存在的串行依赖。当开发暴露真实交叉需求时再调优先级；不为完成路线图而实现暂无价值的功能。

详见 [ENGINE_CAPABILITY_ROADMAP.md](./ENGINE_CAPABILITY_ROADMAP.md)。

---

## Agent behavior

- Bootstrap / 架构讨论 / Feature 规划：以本文件为长期约束，而非仅做局部最优。
- 维护者方案明显滑向 Opinions / 强制 Framework / 过早模仿 UE 完整体系时：**温和提醒**并引用对应原则。
- 与 ACTIVE_WORK 冲突时：以维护者本会话指令为准，但指出与哲学的张力。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | 正式确立；写入 docs/ai + agent always-apply rule |
