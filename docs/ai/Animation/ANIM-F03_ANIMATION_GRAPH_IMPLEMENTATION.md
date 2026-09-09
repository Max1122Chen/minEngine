# ANIM-F03 — Animation Graph MVP — Implementation Plan

## Meta
- **ID:** `ANIM-F03`
- **Type:** Feature
- **Status:** In Progress（S01–S04/S06/S07 Done；S08 Deferred）
- **Owner:** project maintainer
- **Last updated:** 2026-09-06（runtime MVP；`test animation-graph` PASS）
- **Branch:** `feat/animation`
- **Related:**
  - [Design Spec](./ANIM-F03_ANIMATION_GRAPH_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - Params: [CORE-F08 Impl](../Platform/Core/CORE-F08_PARAMETER_STORAGE_IMPLEMENTATION.md)（**S00–S02 Done**）
- **Depends on:** `ANIM-F02` Done；`CORE-F08` S00–S02 Done（no ParamDef bypass）

## TL;DR
按 Design 落地 Mecanim-lite SM Graph：先 `Pose::Blend`，再资产与 Instance，再 SMC / Demo；AnyState 与可选 Attack 随后；`AnimGraphWindow` Deferred。参数只消费 CORE-F08。

## Slice overview

| Slice | Title | Status | DoD（摘要） |
|-------|-------|--------|-------------|
| S00 | CORE-F08-S02 Schema ME_STRUCT embed | **Done** (`99d05b9`) | Graph 可内嵌 Schema；非 Anim 目录实现 |
| S01 | `Pose::Blend` | **Done** | 单测 0 / 0.5 / 1；同骨 TRS Lerp/Slerp |
| S02 | AnimationGraph asset + Loader/Save | **Done** | `.meagraph` + EditorPos；Schema embed |
| S03 | AnimationGraphInstance | **Done** | Set*/SetTrigger/Update/GetStore；条件边 + 过渡 Blend |
| S04 | SMC integration | **Done** | Graph vs Player 互斥；Guid 校验 |
| S05 | Demo Idle↔Walk + Inspector | **Done** | 自动化 Idle↔Walk；Inspector 可赋 Graph；人型目视待维护者 |
| S06 | AnyState runtime | **Done** | AnyState + Trigger consume 单测 |
| S07 | Attack Trigger state | **Done** | 含于 AnyState Trigger 单测 |
| S08 | AnimGraphWindow | Deferred | EditorGraph 投影编辑；非 MVP 阻塞 |

```text
S00 F08-S02 Done
        |
S01 Blend -> S02 Asset -> S03 Instance -> S04 SMC -> S05 Demo
                                    |--> S06 AnyState
                                    |--> S07 Attack (opt)
                                    `--> S08 Window (deferred)
```

---

## S01 — Pose::Blend

**Goal:** 同 Skeleton 两 Pose 按 alpha 混合。

**Touch（预期）：**
- `Pose`（`Blend` 成员 / static）
- Tests：`skeleton-pose` 或新建 `animation-graph` / `pose-blend` suite

**DoD:**
- [ ] Pos/Scale Lerp，Rot Slerp
- [ ] alpha 0 / 1 恒等；0.5 中间；骨数不一致失败
- [ ] `cmake --build build --target minEngineTests` + 相关 suite PASS

**Out:** 曲线、按骨 Mask、Additive

---

## S02 — AnimationGraph asset

**Goal:** 可序列化 Graph 真相（SM + Schema + EditorPos）。

**Touch（预期）：**
- `AnimationGraph` / `AnimStateMachine` / `AnimState` / `AnimTransition` / `AnimCondition`
- AssetType + Loader/Save；meta
- Schema 字段：直接内嵌 `ParameterSchema`（F08-S02 Done）

**DoD:**
- [ ] States/Transitions/Default/EditorPos 往返
- [ ] Load 校验：Clip Skeleton Guid、条件名 ∈ Schema、Clip 非空
- [ ] **无** ParamDef / 第二参数袋

**Out:** 完整图窗（→ S08）；Exit Time 行为

---

## S03 — AnimationGraphInstance

**Goal:** 参数驱动 SM + 过渡双评价 Blend。

**API:** `Bind` / `GetStore` / `SetBool|Int|Float` / `SetTrigger` / `Update(dt, Pose&)`

**DoD:**
- [ ] 非过渡单 Clip；过渡双 Clip + Blend
- [ ] 条件 AND；先匹配先生效；Trigger Consume on fire
- [ ] 单测假 Idle↔Walk（Speed）+ Trigger

**Out:** BlendTree、Layer、AnyState（→ S06）、Exit Time

---

## S04 — SMC integration

**Goal:** 嵌入 Instance；与 Player 互斥。

**DoD:**
- [ ] 有 Graph → Instance；否则 Player
- [ ] Guid 校验；PlayOnAwake → DefaultState
- [ ] 回归：无 Graph 单 Clip 仍播

**Out:** `AnimatorComponent`

---

## S05 — Demo + Inspector

**Goal:** 人型 Idle↔Walk 目视；Inspector 赋 Graph / 调 Speed。

**DoD:**
- [ ] 同 Skeleton 两 Clip + Graph
- [ ] 改 Speed 见过渡
- [ ] 本地 Animations/Sources **不提交**

---

## S06 — AnyState runtime

**Goal:** 启用预留 AnyState 数据；从当前状态匹配 AnyState 出边。

**DoD:**
- [ ] 数据字段与 Runtime 一致；单测至少一条 AnyState 边
- [ ] 不实现 Exit Time

---

## S08 — AnimGraphWindow（Deferred）

**Goal:** Graph 真相 ↔ EditorGraph 投影；ax 画布编辑。

**DoD（开启时）：**
- [ ] 只复用 EditorGraph/Pin/ax；不复用 MaterialEdGraph/NodeDef/MIR
- [ ] 编辑写回 AnimationGraph；Runtime 不读 EditorGraph

**Note:** MVP **不**阻塞 S01–S05。

---

## Deferred / Optional

| Item | Slice | Notes |
|------|-------|-------|
| AnimGraphWindow | S08 | Deferred |
| Attack Trigger demo | S07 | Optional；无 Event |
| Exit Time | — | Design 字段注释；不实现 |
| AnimatorComponent | — | SMC embed 足够 |
| Event / IK / Retarget / BlendTree / Layers | — | Out of F03 |
| Lua Store binding | — | 后议 |

---

## Engineering notes

- **Build dir:** 仓库根 `build/`（不是 `minEngine/build`）
- **Build:** `cmake --build build --target minEngineTests` / `Editor`
- **Tests:** `minEngine/bin/minEngineTests.exe test <suite>`；smoke：`.\scripts\verify.ps1`
- **Encoding:** 文档 / 源码 UTF-8 **no BOM**，换行 **LF**（Windows 上 Write 工具可能写 UTF-16；优先 Python `pathlib.write_text(..., encoding='utf-8', newline='\n')`）
- **Do not commit:** `MyMEProject/Assets/Animations/**`、`Assets/Sources/**`、本地 scene / project / `DefaultMaterial` 改动、`build_*.log`
- **Commits:** 仅在维护者要求时 **draft message → 显式批准** 后执行（git-commit-mentor）；勿擅自 commit

## Pre-flight（编码前）

- [ ] 读 Design Locked #1–13
- [ ] 确认 F08 路径 `Framework/Parameters/`；无第二袋
- [x] S00：F08-S02 已合入 `99d05b9`
- [ ] Go / Defer：用户确认后再开 S01

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-06 | 初版 Implementation Plan：**Planned**；切片 S00–S08 |
| 2026-09-06 | S00 → Done（F08-S02 `99d05b9`）；依赖行解阻 |
| 2026-09-06 | Runtime MVP land: S01-S04/S06/S07; test animation-graph PASS; S08 Deferred |
