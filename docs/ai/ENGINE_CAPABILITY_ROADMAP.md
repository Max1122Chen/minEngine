# minEngine Capability Roadmap（多轨并行）

## Meta
- **ID:** N/A（跨 Feature 阶段路线图）
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Related:** [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md), [ACTIVE_WORK.md](./ACTIVE_WORK.md), [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md)
- **Trust:** Tier A — 高层方向与并行关系；**具体下一刀**仍以 ACTIVE_WORK 为准

## TL;DR

CORE-F05 Play Mode MVP 已收口。下一阶段以 **Animation 主航道** 为核心能力扩展，**2D→UI** 为后续主航道；Infra / Rendering / DX 可并行且不阻塞主线。Gameplay Framework 与 Networking 刻意延后为 Future Capability。排期服从设计哲学，不服从线性 TODO。

## Scope
- **In:** 轨划分、优先级、Capability Milestone、前置条件、过度设计风险、延后项
- **Out:** 单 Feature 切片实现细节；立即开码

## Reader quick start
1. 先读 [ENGINE_DESIGN_PHILOSOPHY.md](./ENGINE_DESIGN_PHILOSOPHY.md)。
2. 看 §2 轨图与 §3 里程碑。
3. 动手前看 [ACTIVE_WORK.md](./ACTIVE_WORK.md) 当前焦点。

---

## 1) 阶段判断（Bootstrap 结论）

### 已具备（可支撑体验扩展）

| 能力域 | 状态摘要 |
|--------|----------|
| RHI / Forward path / Material | 可用；RND-F06 未完但不挡 Animation 起步 |
| Scene / Entity / Component / Reflection | 可用；Activate、展示名已落地 |
| Serialization（JSON + Binary 雏形） | 可用；Binary 协议仍脆弱（TD-028） |
| Play Mode（双 Scene / Inspecting） | MVP Done；Pause/Step、rollback 债延后 |
| Physics / Audio | MVP 级可用 |
| Lua + Script binding | 可用 |
| Editor shell + Console Commands | Console MVP；Workflow 切片未完 |
| Asset 基础 | 有；缺严格 Async Lifecycle |

### 阶段转变

| 从 | 到 |
|----|-----|
| 单系统能力建设为主 | **整体能力扩展 + 完整开发体验** |
| 强线性依赖心态 | **Primary + 并行支线** |
| 偶发模仿大型引擎结构 | **Capabilities / Mechanism / Minimal Core** 约束 |

### 当前 WIP 张力（需显式选择）

Registry 仍有 **In Progress**：`ED-F04`（MVP 已收）、`ED-F01`（VK 阴影 defer）、`RND-F06`、`WF-F02`。  
**推荐：** 不把它们当主航道阻塞；Animation Design 启动前，先选定 ACTIVE_WORK 焦点（见 §7）。

---

## 2) 轨划分与并行关系

```text
PRIMARY ──────── Animation ──► 2D/Sprite ──► UI
                      │
INFRA ───────────────┼── Async Asset / Lifetime / Thread Pool
                     ├── Binary Serialization Protocol
                     ├── Prefab（依赖 Reflection+Ser+Scene 成熟度）
                     └── Object Lifetime / GC（先 ownership，后 GC）
                      │
RENDER ──────────────┼── Collect / Sort / Batch / PSO grouping
                      │
DX / AGENT ──────────┼── Editor Workflow · Reflection UX · Commands
                      │
FUTURE ──────────────┴── Gameplay Plugins · Networking · Net Game slice
```

| Track | 角色 | 是否阻塞 Primary |
|-------|------|------------------|
| **Primary** | Animation → 2D → UI | — |
| **Infrastructure** | Asset / Thread / Ser / Prefab / Lifetime | **否**（除非 Primary 切片碰到真实缺口） |
| **Rendering** | Sorting / Batching | **否** |
| **Developer Experience** | Editor / Debug / Agent 共用控制面 | **否**（小切片可穿插） |
| **Future Capability** | Gameplay Plugins / Networking | 刻意延后 |

---

## 3) Capability Milestones（阶段目标，非 Feature 勾选表）

### M0 — Philosophy & planning locked（本会话）

- [x] 设计哲学落盘 + agent 铭记机制
- [x] 多轨 Capability Roadmap
- [ ] ACTIVE_WORK 对齐新阶段焦点（维护者确认后）

### M1 — Animation vertical slice（Core mechanism）

**Capability：** 可加载/播放骨骼动画并进入渲染管线的最小闭环。

建议能力顺序（机制向，非完整 Framework）：

1. Skeletal Mesh 表示 + 与现有 Mesh/Material 组合
2. Animation Asset + 采样
3. Animation Instance / 当前 Pose
4. Skinning → 与 Renderer 整合
5. Parameters / 简单状态切换（再谈 Graph）

**哲学闸门：** Animation Graph / State Machine 不要在 M1 做成强制完整 Framework；先 mechanism，后可选图。

**前置：** CORE-F05 已满足 Play 验证路径。正式 Design 前更新 `ANIM-F01` Placeholder → Design Spec。  
**非硬前置：** 完整 Async Asset、Prefab、GC。

### M2 — 2D Rendering foundation

**Capability：** Sprite / 2D 变换与绘制路径稳定（`RND-F16`）。

为 UI 铺路，但不提前规定 UI 架构。

### M3 — UI mechanism MVP

**Capability：** Widget 表示、布局、输入、绘制、基础样式、Editor 可见；克制，不复制 UMG/uGUI 全套。

依赖 M2；Gameplay 业务 UI 模式不进 Core。

### M4 — Asset lifecycle rigor（Infra）

**Capability：** Loading → Loaded → Ref → Unref → Unload 的明确所有权与线程边界；不仅是 `LoadAsync` API。

可与 M1 并行；仅当 Animation 资源加载成为真实痛点时插入 Primary。

### M5 — Serialization as infrastructure

**Capability：** Binary 协议字段 ID / 版本 / 兼容 / schema 演进可说明、可测（消化 TD-028/029）。

Prefab 与网络复制的长期底座；不阻塞 Animation 起步。

### M6 — Render efficiency pass

**Capability：** Collect → Sort → Batch → 减少无谓 state change；规则正确优先于微优化。

独立推进；不挡 Animation/UI。

### M7 — Editor / Agent control surface

**Capability：** 打开/创建资产工作流、反射 UX、Command/查询路径与 Runtime 同源。

`ED-F02` 等切片；Agent-friendly 通过改善 Engine API，不另起 Agent Framework。

### M8 — Future：Gameplay plugins + Networking vertical slice

**Capability：** 可选 Gameplay 包 + 小型网络游戏验证对象身份、序列化、世界状态、复制与时序。

仅在 M4/M5 与部分 Primary 成熟后认真开题。

---

## 4) 优先级建议（近期 1–2 个迭代）

| 优先级 | 动作 | 说明 |
|--------|------|------|
| **P0** | 确认 ACTIVE_WORK 主焦点 = Animation（或先收口一小段 DX） | 避免多 In Progress 无主航道 |
| **P0** | `ANIM-F01` 正式 Design（机制边界 + Core/非 Core） | 开码前 Pre-flight |
| **P1** | Animation M1 垂直切片实施 | Primary |
| **P1∥** | TD-028 Binary 协议加固（Infra） | 不挡 Anim；降低 PIE JSON 绕道债 |
| **P1∥** | ED-F02 S00–S01（打开 Scene 工作流） | DX；可穿插 |
| **P2** | RND-F16 设计加深（在 Anim 首个垂直切片可玩后） | Primary 后续 |
| **P2∥** | Render Sort/Batch 设计或小切片 | Rendering track |
| **P3** | Prefab / GC / Gameplay Plugin / Net | 延后；见 §6 |

---

## 5) 架构前置条件 vs 伪依赖

| 需求 | 真前置？ | 说明 |
|------|----------|------|
| Play Mode | ✅ 已有 | 动画运行时验证可用 |
| Reflection / Component | ✅ 已有 | Anim 组件挂接 |
| 完整 Async Asset | ❌ 伪硬依赖 | 同步/简单加载可先支撑 M1 |
| Prefab | ❌ | 建立在 Ser+Scene 成熟后 |
| 完整 GC | ❌ | 先 ownership 模型，再谈 GC 职责 |
| 完整 Anim Graph | ❌ | M1 后可选扩展 |
| UE 式 Gameplay Framework | ❌ 反模式 | 违反 Minimal Core / Opinions |
| Binary 协议完美 | ⚠️ 软前置 | Prefab/Net 前需要；Anim 可暂用现有路径 |

---

## 6) 过度设计风险 & 延后

### 容易过度设计（主动挑战）

| 区域 | 风险 | 默认立场 |
|------|------|----------|
| Animation Graph 全家桶 | 一次做成不可替换 Framework | 先 Pose / Instance / Skinning |
| UI 完整 Framework | 复制 UMG/uGUI | Widget + Layout + Input + Draw |
| Gameplay（Pawn/Character/…） | 把意见写进 Core | Plugin；无实际机制需求不开 |
| 独立 Agent Framework | 与 Engine 双轨 API | 强化 Reflection/Command/Ser |
| 过早 GC | “先有 GC 再说” | 先 ownership / ref 语义 |
| 为 Net 预留一切 | 抽象 levitation | 序列化与身份先做扎实 |

### 明确延后

| 项 | 原因 | Unblock |
|----|------|---------|
| CORE-F05-S05 Pause/Step | 非体验扩展主线 | 维护者指定 |
| ED-F01 VK 阴影质量 | 已 Deferred | 视觉债触发 |
| PHYS-F03 Contact 派发 | Gameplay 倾向 | Delegate 已有；真需求再开 |
| Prefab | Ser + Scene 成熟度 | M5 进展 + 显式 Feature |
| Object GC | ownership 未先澄清 | Lifetime 设计后 |
| Gameplay Framework 大包 | 哲学禁止过早 | Plugin 设计单开 |
| Networking / Net Game | 综合验证，非孤立 API | M4/M5 + 部分 Primary |
| RND-F12 Granite 全复刻 | 卫生项 | 不挡 backlog |

---

## 7) 与 ACTIVE_WORK 的衔接（待维护者确认）

**推荐默认（对齐本纲领）：**

1. **Primary：** `ANIM-F01` — 写正式 Design → Pre-flight → 开 `feat/animation`（或约定分支策略）。
2. **可并行穿插：** `ED-F02` 高优切片（S00/S01）；Binary TD-028 调查/加固。
3. **收口但不升主线：** `ED-F04` 保持 MVP；不主动开 S10b/S07。
4. **Rendering：** Sort/Batch 另开设计时再登记；不阻塞 Anim。

**备选：** 若维护者希望先补齐“打开/创建 Scene”体验再进 Animation，可将 ED-F02 S00–S02 作为短 DX sprint，然后切 Primary。

---

## 8) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | 初版：设计哲学阶段 + 多轨里程碑；Bootstrap 规划，不开码 |
