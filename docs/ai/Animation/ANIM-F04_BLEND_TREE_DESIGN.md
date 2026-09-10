# ANIM-F04 — Blend Tree (1D) — Design Spec

## Meta
- **ID:** ANIM-F04
- **Type:** Feature
- **Status:** Review（待维护者审批；**编码前须 Design Planned+**）
- **Owner:** project maintainer
- **Last updated:** 2026-09-10
- **Branch:** eat/animation（建议独立切片，勿与 ED-F06 并行大改）
- **Related:**
  - [Implementation Plan](./ANIM-F04_BLEND_TREE_IMPLEMENTATION.md)
  - Depends: [ANIM-F03](./ANIM-F03_ANIMATION_GRAPH_DESIGN.md)（FSM + Pose Blend + Params）
  - Editor canvas: [ED-F05](../Editor/ED-F05_STATE_MACHINE_CANVAS_DESIGN.md) / [ED-F06](../Editor/ED-F06_ANIM_SM_CANVAS_POLISH_DESIGN.md)
  - Params: [CORE-F08](../Platform/Core/CORE-F08_PARAMETER_STORAGE_DESIGN.md)
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Depends on:** ANIM-F03 Runtime MVP Done；ParameterStore 可驱动 float
- **Implementation Plan:** [ANIM-F04_BLEND_TREE_IMPLEMENTATION.md](./ANIM-F04_BLEND_TREE_IMPLEMENTATION.md)

## TL;DR
在 Animation Graph 中引入 **Unity 式 1D BlendTree**：State 可挂「多 Clip + 阈值参数阈值阈」混合，输出仍为单 Pose。  
**子状态机（Nested SM）不在本 Feature MVP**；登记为后续 ANIM-F05（见 §Out / ACTIVE_WORK backlog）。

## Scope

### In
- 数据：BlendTree 资产片段或内嵌于 AnimState（阈值阈值阈值阈 Clip 列表 + 绑定 float Param）
- Runtime：AnimationGraphInstance 在 State 评价时若为 BlendTree → 按参数 1D 混合（复用 Pose::Blend 链或 N-way）
- Editor（MVP 可最小）：Inspector 编辑阈值阈/Clip；画布可用「State 标记为 BlendTree」或后置嵌套画布
- 单测：阈值端点 / 中点混合

### Out
- 2D BlendTree / Freeform / Directional
- Additive / Mask / Layer 权重栈
- **Nested State Machine（→ ANIM-F05 Planned backlog）**
- AnimBP 节点 VM / BlendSpace 全套 UX
- Exit Time、Event

## Reader quick start
1. 本文件 §3 数据与求值
2. Impl 切片
3. 代码入口（落地后）：Animation/ + Inspector；画布嵌套 Deferred

---

## 1) 背景与目标

ANIM-F03 每 State 单 Clip；Locomotion 常用 Speed→Idle/Walk/Run 需 1D BlendTree。对标 Unity Blend Tree 1D，保持 Mecanim-lite，不做 UE BlendSpace 全功能。

### 成功长什么样
某 State 类型 = BlendTree1D；绑定 float Speed；阈值阈值阈 Clip；改参数时 Pose 连续变化；Save/Load 往返。

---

## 2) 现状

| 项 | 现状 |
|----|------|
| Pose::Blend | 两 Pose 插值 Done |
| AnimState | 单 Clip + Loop/Speed |
| 参数 | CORE-F08 Schema/Store |
| 画布 | SmGraph 仅普通 State |

---

## 3) 方案（草案 — 待审批锁定）

### 3.1 数据（建议）

`	ext
AnimState
  Kind: Clip | BlendTree1D   // 或 bUseBlendTree
  Clip                       // Kind=Clip 时用
  BlendTree1D?               // Kind=BlendTree1D
    ParamName: string        // float in Schema
    Thresholds: [{ Threshold: float, Clip: AnimationClip }]  // 按 Threshold 排序
`

序列化：嵌入 .meagraph；Validate：Param 存在且为 Float；阈值阈值阈 ≥2；Clip 非空。

### 3.2 Runtime 求值

1. 读 Store float 参数值 x
2. 在排序阈值阈上定位区间 [t_i, t_{i+1}]
3. lpha = (x - t_i) / (t_{i+1} - t_i)（夹紧）
4. 采样两 Clip Pose → Pose::Blend
5. 端外：夹到最近端点 Clip

Transition 期间：仍对「State 输出 Pose」做双 State Blend（与现逻辑一致）。

### 3.3 Editor MVP

| 优先级 | 内容 |
|--------|------|
| P0 | Inspector：切换 Kind；编辑阈值阈列表与 Param |
| P1 | 画布 State 副标题显示 BlendTree / Param |
| P2 | 双击进入子画布编辑阈值阈（Deferred） |

### 3.4 与子状态机边界

Nested SM = 另一套「State 内嵌 StateMachine」求值与画布钻入，**单独 ANIM-F05**，本 Feature 只预留 Kind 扩展点叙述，不实现。

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| A. 独立 .meblend 资产 | 可二期；MVP **内嵌 State** |
| B. 2D 一起做 | Reject（范围爆炸） |
| C. 仅 Editor 假混合 | Reject |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| N-Clip 连续 Blend 分配 | 先双 Clip 区间；避免每帧堆分配 |
| Schema 改动破坏旧图 | Kind 默认 Clip；旧资产兼容 |
| 与 ED-F06 并行冲突 | **先 ED-F06 或严格分文件** |

---

## 6) 验收

- [ ] State 可配置 1D BlendTree；Validate 合理
- [ ] 参数变化 Pose 连续；单测覆盖端点/中点
- [ ] .meagraph Save/Load
- [ ] 旧单 Clip State 行为不变
- [ ] 文档 / Registry 对齐；Nested SM 未偷偷实现

---

## 7) Status note

| 字段 | 内容 |
|------|------|
| Status | **Review** |
| What's done | Design/Impl 草案 |
| Next | 审批锁定数据形状 → 再实现 |
| Blocked by | 审批；建议 ED-F06 优先或错峰 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-10 | 初稿：1D BlendTree；Nested SM → ANIM-F05 backlog |
