# ANIM-F03 — Animation Graph MVP — Design Spec

## Meta
- **ID:** `ANIM-F03`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-10（空 State Clip 允许 + hold-last Pose；ED-F06 画布）
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

**校验（Load / Bind）：** 有 Clip 的 State 之间 Skeleton Guid 一致；条件名 ∈ Schema；**State.Clip 允许为空**（占位 State，连通与否不限）。空 Clip **不是** Validate 失败，也不刷 Warning。

**空 State Runtime（Locked 2026-09-10 — 策略 B）：**
- 允许进入无 Clip 的 State（状态机语义照常）。
- 采样：输出 **上一帧有效 Pose**（hold last）；若尚从未有过有效 Pose，则用图中任一 Clip 的 Skeleton **bind/rest**；再无则空 Pose。
- **不**因空 Clip 报 Error/Warn；不「跳过进入」空 State。
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
| **S08** | Anim Graph Editor 初版（Graph + Details窗 + Parameters） | **Done** (code MVP) |
| **S08b** | Inspector 复用；取消 DetailsWindow；ax SM 伪装 L1–L2/L4；Schema 列宽 | **Done** (code) |

```text
S00 F08-S02 Done
    \
     -> S01 Blend -> S02 Asset -> S03 Instance -> S04 SMC -> S05 Demo
                                              \-> S06 AnyState
                                              \-> S07 Attack (opt)
                                              \-> S08 Editor MVP
                                                   \-> S08b Inspector+SM disguise (Planned)
```


## 9) Animation Graph Editor（S08 / S08b）— UI 与交互

> 对标 Material：`*Window` + Dock + Session + **共享 Inspector**；画布用 `imgui-node-editor`。  
> **真源**仍是 `AnimationGraph`（StateMachine + Schema）；ax Node/Pin/Link = **投影**，可丢弃重建。

### 9.0 产品隐喻（Locked）

| | 我们 | 不是 |
|--|------|------|
| Runtime | Mecanim-lite：**`DefaultStateName` = 逻辑 Entry**；当前 State 的 Clip → Pose | UE AnimBP：**Output Pose** 节点 / AnimNode VM |
| Editor 目标感 | **状态机图**（有向边、边缘拖线、点边看 Transition） | Material 式多类型数据流节点图 |
| 实现策略 | **ax 渐进伪装**（仍用 Pin/Link 基板）；**不**为本 Feature 自研 SM 画布 | 一口吃成 UE 级边中规则菱形 / 完美双向路由 |

**播放机制（编辑器无关，写清以免再误解）：**  
`AnimationGraphInstance` 每帧在当前 State 上评价 Clip → Pose；Transition 期间双 Clip + `Pose::Blend`。SMC 有 Graph 则走 Instance，否则 `AnimationPlayer`。图上**不需要** Output Pose 节点。

### 9.1 产品目标

打开 `.meagraph`：可视化编辑 State / Transition；Schema 固定可编辑；选中详情走 **Inspector**（与 Material 同壳）。  
Runtime 行为不变。

### 9.2 命名与模块

| 角色 | 名称 | 说明 |
|------|------|------|
| SubEditor / Session | `AnimationGraphEditor` | OpenAsset、Dirty、Save、Validate；**不是** Window |
| 画布窗 | `AnimGraphWindow` | SmGraph L1 + `AnimGraphSmBridge`（ED-F05/F06；已离 ax Pin/Link） |
| 选中详情 | **共享 `Inspector` + `AnimGraphInspectorSource`** | 对齐 Material；**取消**独立 `AnimGraphDetailsWindow` |
| 参数声明窗 | `AnimGraphParametersWindow` | **固定**整图 `ParameterSchema`（与选中无关） |
| 预览窗 | `AnimGraphPreviewWindow` | **Deferred** |
| 画布语义 | Entry / AnyState / 边菜单 | 见 [ED-F06](../Editor/ED-F06_ANIM_SM_CANVAS_POLISH_DESIGN.md)（`AnimGraphIds` 已删） |

**取消独立 DetailsWindow（Locked 2026-09-09）：** 曾落地的 `AnimGraphDetailsWindow` 在 **S08b** 删除；逻辑迁入 `AnimGraphInspectorSource`（`GetInspectorSource()` 非空）。

### 9.3 Dock 布局（Locked — 修订）

`BuildAnimationGraphEditingLayout()`：

```text
┌─ AnimGraphWindow（左 ~60–65%）─────────────────────────────────────┐
│ Toolbar: [Graph] [Validate] [Save] [Add State] [New Graph] *dirty   │
│  ax：State 块 + 有向 Transition 边（Pin 作边缘热区，弱化数据流感）   │
└──────────────────────────────────────────────────────────────────────┘
┌─ Inspector（右上 ~35%）──────────────┐
│ AnimGraphInspectorSource             │
│ 选中 State / Transition / 无选中     │
└──────────────────────────────────────┘
┌─ AnimGraphParametersWindow（右下）──┐
│ Schema：Name | Type | Default        │
└──────────────────────────────────────┘
(+ 可选 Console 底栏，对齐 Material)
```

| # | 决策 |
|---|------|
| L1 | 右栏：**上 Inspector、下 Parameters**（不再用独立 Details 窗） |
| L2 | Parameters **固定**右下；不随选中变空 |
| L3 | 仅 `AnimGraphWindow` 调用 `ax::NodeEditor::Begin/End` |
| L4 | Scene 套件窗在 Anim 模式下关闭（同 Material） |
| L5 | Preview **Out of S08/S08b MVP** |
| L6 | 共享 Inspector **必须**在 Anim 模式由 `AnimGraphInspectorSource` 供稿；勿留空壳无 Source |

### 9.4 打开 / 保存 / Dirty

（同前：ContentBrowser → TryOpenAsset → `AnimationGraphEditor::OpenAsset` → Activate SubModule → Session。）

| 操作 | 行为 |
|------|------|
| 改 State/Transition/Schema/连线/位置 | `NotifyGraphChanged()` → Dirty |
| Save | `AssetManager::SaveAsset` / Loader；清 Dirty |
| Validate | `AnimationGraph::Validate`；日志 / Console |
| 切模式 / 退出 | Dirty 提示（对齐 Material） |

### 9.5 画布：状态机伪装分层（S08b Locked）

真源不变：`Transitions[].From/To`；**禁止**第二套边权威。

| 层 | 内容 | 切片 |
|----|------|------|
| **L1** | Link **箭头**表方向；点选 Link → Inspector 显示 Transition；命中可点 | **S08b** |
| **L2** | Pin **收成节点边缘热区**（弱化 In/Out 标签与数据流观感）；从热区拖出 = Create Transition；禁止自环 | **S08b** |
| **L3** | 可选 **Entry** 装饰节点（只读，指向 `DefaultStateName`）；改 Default 只在 Inspector 无选中时 | S08b 可做 / 可紧随 |
| **L4** | Inspector：**Reverse**（交换 From/To）；To 可改；Conditions 表 | **S08b** |
| **L5** | 边中段规则图标、花式双向曲线、完全自定义 SM 画布 | **Deferred**（非本 Feature 必达） |

**节点：** 标题 = State `Name`；副标题可选 Clip；位置 → `EditorPos*`；`SettingsFile = nullptr`。  
**删边 / 删节点：** 写回真源（含清理关联 Transition / AnyState 边）。  
**AnyState：** 实体节点 UX 仍可后置；工具栏/列表入口可保留。

### 9.6 Inspector（选中驱动 — 原 Details）

由 `AnimGraphInspectorSource::DrawInspector` 绘制（Material 同模式）：

| 选中 | 显示 |
|------|------|
| State | Name（Rename）、Clip、bLoop、Speed |
| Transition | From（只读）、To、BlendDuration、Conditions；**Reverse** |
| 无选中 | 提示 + **DefaultState** 下拉（逻辑 Entry） |

Condition.`ParamName` ∈ Schema；Schema 改名后未更新的条件 Validate 失败。

### 9.7 Parameters 窗（固定 Schema）

| 能力 | 要求 |
|------|------|
| 列宽 | **Locked：** Name **~40%** / Type **~25%** / Default **~35%**（总和 100%；`WidthStretch` 权重，禁止 Default 无约束撑爆） |
| 列表 | Name / Type（Bool·Int32·Float）/ Default + 删除 |
| Add / Remove / 改 Type·Default | 写 `ParameterSchema`；Remove 若 Condition 仍引用 → WARN |

**不**在此窗编辑运行时 Store（Preview/Play Deferred）。

### 9.8 与 Material 的同 / 异（修订）

| | Material | Anim Graph（S08b） |
|--|----------|-------------------|
| 模式 | Material SubModule | AnimationGraph SubModule |
| 选中详情 | **共享 Inspector** + `MaterialEditorInspectorSource` | **共享 Inspector** + `AnimGraphInspectorSource` |
| 右栏另窗 | （无独立 Details）+ Preview 在左 | **Parameters** 固定右下（Schema） |
| 画布隐喻 | 多类型 NodeDef + typed Pin | **SM 伪装**：边缘热区 + 有向边 |
| 改图后果 | Compile | Validate；无 GPU compile |
| 复用边界 | — | **不**复用 MaterialEdGraph / IR / NodeDef |

### 9.9 非目标

- Preview 视口（S08c / 另议）
- L5 完整 UE 边中规则 UX / 自研 SM 画布
- BlendTree / 子状态机 / Exit Time UI
- 把 Parameters 塞进 Inspector 折叠（**否决**：Schema 独立窗）
- 重新引入独立 `AnimGraphDetailsWindow`

### 9.10 验收

**S08（已有代码基线）：**
- [x] 打开/保存 `.meagraph`；Dirty；Graph + Schema 窗；真源 = AnimationGraph
- [x] 拖线增删 Transition；拖节点 EditorPos（Pin 式 MVP）

**S08b（本修订落地后）：**
- [x] 删除 `AnimGraphDetailsWindow`；`GetInspectorSource()` → `AnimGraphInspectorSource`
- [x] Dock：Graph | Inspector（上）| Parameters（下）；无空 Inspector
- [x] Schema 列宽约 40/25/35，Name/Type 可读
- [x] L1–L2：Flow 方向标记 + 边缘热区拖线 + 点选边进 Inspector
- [x] L4：Inspector 可 Reverse Transition
- [x] 仍无 Material IR；真源仅 `AnimationGraph`


## 8) Status note

| 字段 | 内容 |
|------|------|
| Status | **In Progress** |
| What's done | Runtime MVP `a59b79a`；S08 MVP + **S08b**（Inspector / SM 伪装 / Schema 列宽）；Editor Debug rebuild PASS |
| What's not | 手动 smoke；Preview Deferred；人型目视；SM L5 |
| Next | S08b 已合入；真·SM 见 ED-F05 Review |
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
| 2026-09-09 | S08 设计：三窗 Dock（Graph + Details 右上 + Parameters Schema 右下）；交互对齐 Material |
| 2026-09-09 | S08 Editor MVP code land：三窗 + OpenAsset/Save；Editor build PASS |
| 2026-09-09 | §9 修订：**复用 Inspector**、**取消 AnimGraphDetailsWindow**、ax **SM 伪装** L1–L2/L4、Schema 列宽 40/25/35；切片 **S08b** |
| 2026-09-09 | S08b code land：InspectorSource、删 DetailsWindow、Flow/边缘热区、Reverse、Schema 40/25/35 |
| 2026-09-09 | 真·SM 画布开项 **ED-F05**（复用 ImGuiEx::Canvas；待审批）；本 Feature 真源不变 |
