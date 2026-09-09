# ANIM-F03 — Animation Graph MVP — Design Spec

## Meta
- **ID:** `ANIM-F03`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-06（runtime MVP；`test animation-graph` PASS）
- **Branch:** `feat/animation`
- **Related:**
  - [Implementation Plan](./ANIM-F03_ANIMATION_GRAPH_IMPLEMENTATION.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - Prerequisite: [ANIM-F01](./ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md)（Done）· [ANIM-F02](./ANIM-F02_CLIP_PLAYBACK_DESIGN.md)（Done）
  - Shared params: [CORE-F08 Design](../Platform/Core/CORE-F08_PARAMETER_STORAGE_DESIGN.md) · [Impl](../Platform/Core/CORE-F08_PARAMETER_STORAGE_IMPLEMENTATION.md)
  - Asset Import: [ASSET-F02](../Asset/ASSET-F02_IMPORT_SERVICE_DESIGN.md)（Done；多 Clip 共用 Skeleton）
  - Editor substrate: `Runtime/Function/EditorGraph/`（与 Material 同 canvas 能力，不同 schema）
- **Depends on:**
  - `ANIM-F02` — **Done**（`AnimationClip` / `AnimationPlayer` / SMC 单 Clip 路径）
  - `CORE-F08` S00–S02 — **Done**（含 Schema `ME_STRUCT` JSON 往返 `99d05b9`；Graph 可直接内嵌 `ParameterSchema`；**禁止** ParamDef 旁路）
- **Implementation Plan:** [ANIM-F03_ANIMATION_GRAPH_IMPLEMENTATION.md](./ANIM-F03_ANIMATION_GRAPH_IMPLEMENTATION.md)

## TL;DR
定位 **Unity Mecanim-lite FSM**（State + Transition + Params），**不是** UE AnimBP 节点 VM。  
资产从 Day-1 即可编辑（State / Transition 带 `EditorPos`）；**Runtime 真相 = `AnimStateMachine` 数据**；`EditorGraph` + Pin = 编辑/呈现基板（与 Material 同 canvas 能力，不同 schema）。  
参数只走 **CORE-F08** `ParameterStore`。`AnimationGraphInstance` 与 `AnimationPlayer` 是 **并行 Pose 生产者**；`SkeletalMeshComponent`（SMC）二选一。

## Scope

### In
- 资产：`AnimationGraph`（内嵌 / 引用 `ParameterSchema`、States、Transitions、DefaultState；`EditorPos`）
- Runtime：`AnimationGraphInstance`（Store + SM + 过渡双 Clip 评价 + `Pose::Blend`）→ Pose
- 参数：消费 CORE-F08；Trigger = Anim 层 Bool Raise/Consume（非 F08 新 ValueType）
- SMC：有 Graph → Instance；否则 → F02 Player
- EditorGraph 投影：Graph 真相 → EditorGraph 呈现（AnimGraphWindow 可分阶段）
- Demo：同 Skeleton Idle↔Walk（`Speed` + blend）；单测 Blend / 条件 / Trigger

### Out（分阶段切片 / 不进 MVP 主路径）
- 完整 AnimBP 节点 VM / BlendTree / Layers / Mask / Additive / Montage
- Animation Event / IK / Root Motion / Retarget
- 第二套参数袋 / 完整 Blackboard 产品
- 独立 `AnimatorComponent`（SMC 内嵌优先）
- 完整图编辑器 UX 作为 Day-1 阻塞（`AnimGraphWindow` = S08 Deferred；数据层先可序列化）

## Locked decisions

| # | 决策 | 说明 |
|---|------|------|
| 1 | 产品形态 | **Unity-like FSM**（State + Transition + Params）；**非** UE AnimBP VM |
| 2 | Graph-is-product | 资产从 Day-1 可编辑；`EditorPos` 进资产；真相在 SM 数据，非临时表 |
| 3 | 并行生产者 | `AnimationGraphInstance` \|\| `AnimationPlayer`；SMC 选一；**不**把 SM 塞进 Player |
| 4 | SMC 挂载 | Graph Instance **嵌入** SMC；独立 `AnimatorComponent` **Deferred** |
| 5 | 参数 | **仅 CORE-F08**；禁止 Anim 自研 ParamDef / 第二袋 |
| 6 | Trigger | Anim 策略：`Raise` / `Consume`（建议基于 Bool 槽）；匹配成功边后 Consume |
| 7 | 同骨 | 图内 Clip **同一** `.meskeleton` Guid；引擎不做 Retarget |
| 8 | 缺轨 | 评价前 `FillBind` + 覆盖有轨分量（同 F02） |
| 9 | TRS Blend | Pos/Scale **Lerp**，Rot **Slerp**；`alpha` 对 `blendTime` **线性**（曲线后议） |
| 10 | AnyState / Entry | **数据字段预留**；Runtime 可分阶段启用（S06） |
| 11 | Exit Time | **Deferred**（字段可注释预留，不实现） |
| 12 | EditorGraph | **复用** `EditorGraph` / Pin / ax 画布；Pin = **视觉连接**；不复用 MaterialEdGraph / NodeDef / MIR |
| 13 | Event | **Out of F03**（建议后续 Feature） |

## Reader quick start
1. 本文件：§0 定位 · Locked · §3 数据流 / 结构 / SMC / Editor 边界
2. [Implementation Plan](./ANIM-F03_ANIMATION_GRAPH_IMPLEMENTATION.md) — S00–S08
3. 代码入口（实现后）：`Animation/` Graph + `Framework/Parameters/` + `EditorGraph/`

---
## 0) Product positioning（产品定位）

### 0.1 Unity vs AnimBP vs UEdGraph family

| 参照系 | 我们学什么 | 我们不学什么 |
|--------|------------|--------------|
| **Unity Mecanim / Animator Controller** | FSM：State、Transition、Parameters、AnyState 语义；Controller 即资产 | 完整 BlendTree 图、Layer 权重栈、Avatar Mask 全套 |
| **UE AnimBP（AnimInstance VM）** | Pose 产出进 Component 的直觉 | 节点图 VM、AnimGraph 编译、AnimNode 网络求值 |
| **UE UEdGraph 家族（EdGraph / Schema）** | 编辑器图 = **呈现与编辑**；运行时另有真相数据 | 把编辑器节点直接当 Runtime 执行图 |

结论：F03 = **Mecanim-lite 数据驱动 SM** + **可编辑图资产**；EditorGraph 是 Material 同级的 **canvas 基板**，不是 AnimBP。

### 0.2 CORE-F08 note
- 代码目录（已锁）：`Runtime/Function/Framework/Parameters/`
- Graph **只消费** Schema / Layout / Store；**不**实现第二参数袋
- Schema 写入 Graph 资产：直接内嵌 CORE-F08 `ParameterSchema`（S02 Done）；**禁止** ParamDef 旁路
- Trigger **不是** F08 ValueType；在 Anim 策略层用 Bool Raise/Consume

### 0.3 Glossary

| Term | Meaning |
|------|---------|
| AnimationGraph | 资产：Schema + StateMachine + 编辑元数据 |
| AnimStateMachine | Runtime 真相：States / Transitions / Default |
| AnimState | 一状态 → 一 Clip（MVP）；含 `EditorPos` |
| AnimTransition | From→To + Conditions + BlendDuration |
| AnimCondition | 对 Store 的比较 / Trigger IsSet |
| AnimationGraphInstance | Runtime：Store + SM 时钟 + Blend |
| EditorGraph / Pin | 编辑呈现基板；Pin 为视觉连接，非 Material MIR |
| Pose producer | Player **或** GraphInstance；SMC 选一 |

---

## 1) 背景与目标

### 1.1 现状
- F01：Skeleton / Pose / GPU skinning
- F02：单 Clip + `AnimationPlayer` ⊂ SMC；多 Clip 可共用同一 Skeleton（Import 显式选择）
- CORE-F08 S00–S02 **Done**（Schema 可内嵌序列化）
- 无 SM、无 Pose Blend、无 Graph 资产、无 Anim 图窗

### 1.2 目标
Gameplay 通过参数驱动状态切换；过渡期视觉连续；输出仍是 **Pose → 既有 palette 路径**。

**成功标准：**
1. 同 Skeleton 两 Clip + 一 Graph；`Speed` 跨阈值时过渡混合可目视
2. 参数只经 CORE-F08 Store；条件只读 Store
3. 未赋 Graph 时 F02 单 Clip 路径仍可用
4. 资产可保存 `EditorPos`；真相可投影到 EditorGraph（窗体可后置）

### 1.3 与 Brief 对齐
对应 Brief **Step 5+**（Pose Blend + Animator/SM）；**Step 7 Event** 不进 F03。到达 Idle/Walk（可选 Attack Trigger 仍无 Event）后克制扩展。

---

## 2) 平行产品线（F02 vs F03）

| 轴 | F02 Clip Playback | F03 Animation Graph |
|----|-------------------|---------------------|
| 共享参数 | （无） | **CORE-F08** Schema / Store |
| Asset | `AnimationClip` | **`AnimationGraph`**（引用多个 Clip） |
| Runtime | `AnimationPlayer` | **`AnimationGraphInstance`** |
| Component | Player 驱动 Pose | Instance **或** Player（互斥） |
| 编辑 | Inspector / Import | SM 数据 + EditorGraph 投影（窗体分阶段） |
| Render | 无改 | **无改** |

---
## 3) 方案

### 3.1 Runtime dataflow

```text
Gameplay / Editor / (future Lua)
        |
        |  SetFloat / SetBool / SetInt / SetTrigger
        v
  ParameterStore  <── CORE-F08 (Layout from Schema)
        |
        v
  AnimationGraphInstance
   |- evaluate transition conditions (read Store)
   |- advance state / blend alpha
   |- ClipA.Evaluate(tA) -> PoseA
   |- ClipB.Evaluate(tB) -> PoseB   (only while blending)
   `- Pose::Blend(PoseA, PoseB, alpha) -> outPose
        |
        v
  SkeletalMeshComponent -> bone palette -> GPU

No Graph assigned:
  AnimationPlayer.Update -> Clip.Evaluate -> Pose
```

### 3.2 Edit vs truth dataflow

```text
  AnimationGraph (asset truth)
       |
       |  AnimStateMachine + Schema + EditorPos
       |  (serialize / load; Schema embed via CORE-F08 ME_STRUCT)
       v
  EditorGraph projection
       |  nodes/pins for States & Transitions (visual only)
       |  same canvas stack as Material (EditorGraph + Pin + ax)
       |  NOT MaterialEdGraph / NodeDef / MIR
       v
  AnimGraphWindow (S08; may be Deferred)
       |
       `- edits write back to AnimationGraph truth
```

**Invariant:** Runtime 只读 SM 数据 + Store；EditorGraph 可丢弃重建，只要真相资产完整。

### 3.3 Data structures（示意）

```text
AnimationGraph : Asset
  Schema: ParameterSchema          // embed CORE-F08 ME_STRUCT; no ParamDef bypass
  Machine: AnimStateMachine
  // optional: Skeleton ref for validation

AnimStateMachine
  States: AnimState[]
  Transitions: AnimTransition[]
  DefaultStateId: Id
  // Reserved (data ok; runtime phased):
  //   EntryStateId / bHasEntry
  //   AnyState enabled flag

AnimState
  Id / Name
  Clip: shared_ptr<AnimationClip>  // ObjectPtr on disk
  bLoop: bool = true
  EditorPos: Vec2                 // Day-1 editable graph
  // Reserved: bIsAnyStateSink / Entry marker — runtime can phase (S06)

AnimTransition
  Id
  FromStateId / ToStateId         // AnyState: From = reserved sentinel (S06)
  Conditions: AnimCondition[]     // AND
  BlendDurationSeconds: float
  EditorPos: Vec2                 // optional anchor for edge label
  // Deferred — Exit Time (do not implement in MVP):
  //   bHasExitTime
  //   ExitTimeNormalized
  //   bFixedDuration

AnimCondition
  ParamName or KeyId
  Op: > >= < <= == != | IsSet (Trigger)
  Operand: Bool / Int / Float (by type)
```

**校验（Load）：** 全 Clip Skeleton Guid 一致；条件名 ∈ Schema；State Clip 非空。  
扩展名建议：`.meagraph`（以 AssetTypeRegistry 为准）。

### 3.4 Runtime APIs sketch

```cpp
// Prefer Pose member / static — avoid anonymous free helpers when member fits
struct Pose {
  static bool Blend(const Pose& a, const Pose& b, float alpha, Pose& out);
  // Pos/Scale Lerp, Rot Slerp; requires same bone count
};

class AnimationGraphInstance {
public:
  bool Bind(const std::shared_ptr<AnimationGraph>& graph);
  ParameterStore& GetStore();

  void SetBool(std::string_view name, bool v);
  void SetInt(std::string_view name, int32_t v);
  void SetFloat(std::string_view name, float v);
  void SetTrigger(std::string_view name);   // Raise

  void Update(float dt, Pose& outPose);
  // Consume triggers when a matching transition fires
};
```

### 3.5 Instance behavior steps

1. `Bind`：Compile Schema → Layout；`Store.BindLayout` / Reset defaults；进入 `DefaultState`；清过渡。
2. 每帧 `Update(dt)`：
   - 若 **非过渡**：推进当前状态时间；`Clip.Evaluate`（先 `FillBind`）；按声明顺序检查出边，先匹配先生效 → 进入过渡（记录 From/To、双时钟、`blendT=0`）；Trigger 条件边在 **进入过渡时 Consume**。
   - 若 **过渡中**：推进 `blendT`；双评价 → `Pose::Blend`；`alpha = saturate(blendT / duration)`；结束则切到 ToState，单评价。
3. 不负责 RHI / Assimp / Gameplay AI。

### 3.6 Pose Blend rules
- `alpha ∈ [0,1]`；同骨数（同 Skeleton）；否则失败
- TRS：Pos/Scale Lerp，Rot Slerp
- 单测：`alpha = 0 / 0.5 / 1`

### 3.7 SMC integration

```text
if (m_AnimationGraph)
  m_GraphInstance.Update(dt, m_LocalPose)
else
  m_AnimationPlayer path (F02)
```

- Guid：Graph 内所有 Clip 与 Mesh Skeleton 一致
- Inspector：赋 Graph；调试 SetFloat（Speed）
- PlayOnAwake：有 Graph → 从 DefaultState 播

### 3.8 EditorGraph reuse boundary

| 复用 | 不复用 |
|------|--------|
| `EditorGraph` | `MaterialEdGraph` |
| `EditorGraphNode` / Pin | Material `NodeDef` / pin value types as Anim MIR |
| ax 节点画布能力 | Material 编译 / MIR / shader codegen |
| 视觉连线（State↔Transition 呈现） | 把 Pin 当 Runtime 求值边 |

Pin = **visual / presentation**；Runtime 边在 `AnimTransition` 数组。

### 3.9 Player vs Instance

| | AnimationPlayer | AnimationGraphInstance |
|--|-----------------|------------------------|
| 输入 | 单个 Clip | Graph 资产 + 参数 |
| 时间 | 一时钟 | 每状态一时钟；过渡两时钟 |
| 输出 | Pose | Pose（可 Blend） |
| SMC | 无 Graph 时 | 有 Graph 时 |
| 实现 | 不内嵌 SM | 调用 `Clip::Evaluate`；可选用 Player 作辅助但非必须 |

---
## 4) Tests / Demo

| 层级 | 内容 |
|------|------|
| 单测 | `Pose::Blend`；假 Graph Idle↔Walk 条件边；Trigger Raise/Consume；Store Set/Get |
| Editor | 人型：两 Clip 同 Skeleton + Graph；改 Speed 看过渡 |
| 回归 | 无 Graph 时单 Clip 仍播；`test smoke` / `animation-clip` / `parameter-store` |

## 5) Risks

| 风险 | 缓解 |
|------|------|
| 做成半套 Blackboard / 第二参数袋 | Locked #5；目录不进 Animation；无 ParamDef bypass |
| Player+Graph 双路径永久分叉 | Design 写明互斥；未来可 Deprecated 单 Clip UX，不合并进 Player |
| Schema 嵌入做错旁路 | 只用 CORE-F08 `ParameterSchema` ME_STRUCT；无 ParamDef |
| 过早完整图窗拖垮 MVP | 数据 + EditorPos 先；AnimGraphWindow = S08 Deferred |
| AnyState / Exit Time 范围蔓延 | 字段预留；Exit Time Deferred；AnyState Runtime = S06 |
| 把 Material MIR 拖进 Anim | §3.8 边界表；只复用 EditorGraph/Pin/ax |

## 6) Acceptance checklist

- [ ] 参数读写仅经 CORE-F08 `ParameterStore`；无第二袋 / ParamDef bypass
- [ ] `AnimationGraph` 可序列化（含 `EditorPos`）；Schema 内嵌 CORE-F08 `ParameterSchema`
- [ ] Instance 可按参数切换状态；过渡 Pose Blend 可单测 / 可目视
- [ ] SMC：Graph 与单 Clip 路径互斥清晰；同 Skeleton Guid 校验
- [ ] Demo Idle↔Walk PASS
- [ ] EditorGraph 复用边界遵守（无 MaterialEdGraph/MIR 耦合）
- [ ] Out 清单未偷偷实现（Event / BlendTree / AnimatorComponent / 完整图 UX）
- [ ] Design / Impl / Registry / ACTIVE_WORK / Progress 对齐

## 7) Slice preview

| Slice | 内容 | 说明 |
|-------|------|------|
| **S00** | CORE-F08-S02 Schema `ME_STRUCT` 嵌入 | **Done**（`99d05b9`）；解阻 Graph 内嵌 Schema |
| **S01** | `Pose::Blend` + 单测 | |
| **S02** | `AnimationGraph` 资产 + Loader/Save（含 EditorPos） | Schema embed 对齐 S00 |
| **S03** | `AnimationGraphInstance` SM + 过渡混合 | Set* / SetTrigger / Update / GetStore |
| **S04** | SMC 接入 + Guid 校验 | |
| **S05** | Demo Idle↔Walk + 最小 Inspector | |
| **S06** | AnyState Runtime（数据已预留） | |
| **S07** | Attack Trigger 状态（可选；仍无 Event） | Optional |
| **S08** | `AnimGraphWindow`（EditorGraph 投影） | **Deferred**（非 Day-1 阻塞） |

```text
S00 F08-S02 Done
    \
     -> S01 Blend -> S02 Asset -> S03 Instance -> S04 SMC -> S05 Demo
                                              \-> S06 AnyState
                                              \-> S07 Attack (opt)
                                              \-> S08 AnimGraphWindow (deferred)
```

## 8) Status note

| 字段 | 内容 |
|------|------|
| Status | **In Progress** |
| What's done | Runtime MVP：Blend / Graph / Instance / SMC / AnyState+Trigger；单测 PASS |
| What's not | AnimGraphWindow（S08）Deferred；人型 Graph 目视待维护者 |
| Next | 准备 commit；可选人型 Demo 目视 |
| Blocked by | 无 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry Placeholder：SM + Params + Transition Blend |
| 2026-09-05 | 正式 Design Draft：ParameterStore 共享层；Graph MVP；Event/完整 Blackboard Out |
| 2026-09-05 | 参数实现抽出为 **CORE-F08**；本 Feature 改为依赖 Layout/Store |
| 2026-09-06 | CORE-F08 Done 解阻；Status note / 依赖行同步 |
| 2026-09-06 | **Final expansion：** Status→**Planned**；Unity Mecanim-lite；Graph-is-product；EditorGraph 投影；Instance \|\| Player；切片 S00–S08 |
| 2026-09-06 | CORE-F08-S02 已合入（`99d05b9`）；Depends / S00 / Status note 同步为解阻 |
| 2026-09-06 | Runtime MVP land：S01–S04/S06/S07；`test animation-graph` PASS；Status → In Progress |
