# ANIM-F03 — Animation Graph MVP — Design Spec

## Meta
- **ID:** `ANIM-F03`
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-06（CORE-F08 已 Done；本 Feature 可开 Impl）
- **Branch:** `feat/animation`
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - Prerequisite: [ANIM-F01](./ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md)（Done）· [ANIM-F02](./ANIM-F02_CLIP_PLAYBACK_DESIGN.md)（Done）
  - Shared params: [CORE-F08 Design](../Platform/Core/CORE-F08_PARAMETER_STORAGE_DESIGN.md) · [Impl](../Platform/Core/CORE-F08_PARAMETER_STORAGE_IMPLEMENTATION.md)（**已落地**）
  - Asset Import: [ASSET-F02](../Asset/ASSET-F02_IMPORT_SERVICE_DESIGN.md)（Done；多 Clip 共用 Skeleton）
  - Input brief: [`docs/external/minEngine — 3D Animation System Development Brief.md`](../../external/minEngine%20—%203D%20Animation%20System%20Development%20Brief.md)
- **Depends on:** `ANIM-F02`（已满足）；`CORE-F08`（**Done** — Parameter Schema/Layout/Store）
- **Implementation Plan:** 审阅通过后补 `ANIM-F03_ANIMATION_GRAPH_IMPLEMENTATION.md`

## TL;DR
把「单 Clip Player」升级为 **参数驱动的轻量 Animation Graph**（State Machine + Transition + Pose Blend），定位接近精简 Animator Controller，**不是** UE AnimBP 节点 VM。  
参数存储 **不**在本 Feature 实现：依赖 **CORE-F08**（Schema→Layout→Store）。Graph 内嵌/引用 `ParameterSchema`，Instance 持有 `ParameterStore`；Trigger 消费为 Anim 策略层。

## Scope
- **In:**
  - 参数：消费 **CORE-F08** `ParameterSchema` / `ParameterStore`（Bool/Int/Float）；Trigger = Anim 层策略
  - 资产：`AnimationGraph`（参数声明、State→Clip、Transition+条件+blendTime）
  - Runtime：Graph Instance（参数袋 + SM + 过渡双 Clip 评价 + Pose Blend）→ Pose
  - 接入：`SkeletalMeshComponent` 在赋 Graph 时优先走 Graph；否则保留 F02 单 Clip Player
  - Demo：同一 Skeleton 上 Idle↔Walk（`Speed` float + 过渡混合）
  - 单测：Pose Blend、条件过渡、参数 Set/Get/Trigger consume
- **Out:**
  - 完整节点图编辑器 / AnimBP VM / Blend Tree / Layer / Mask / Additive / Montage
  - Animation Event（建议后续 `ANIM-F04`）
  - IK / Root Motion / Retarget / 引擎内重定向
  - Parameter 基础设施实现（→ **CORE-F08**）
  - 完整 AI Blackboard（Observer / Object / Synced / 动态键）
  - Lua ScriptBinding 必做（C++ API 先落地；Lua 可标后续切片）

## Locked decisions

| # | 决策 | 说明 |
|---|------|------|
| 1 | 产品形态 | **数据驱动 SM Graph**，非节点 VM |
| 2 | Player vs Graph | **保留** `AnimationPlayer`（单 Clip）；Graph **不**塞进 Player；过渡期由 Graph 持双时钟并 Blend |
| 3 | Component 挂载 | Graph Instance **组合进** `SkeletalMeshComponent`（有 Graph 则走 Graph，否则走 Player）。独立 `AnimatorComponent` **Deferred** |
| 4 | 参数基础设施 | **依赖 CORE-F08**；Anim 与未来 Blackboard **共享 Layout/Store**，不共享 Blackboard 产品 |
| 5 | 值类型 MVP | Store：`Bool`/`Int`/`Float`；**Trigger** = Anim 对 Bool（或专用 API）的 raise/consume |
| 6 | 多 Clip 同骨 | 各 Clip Import 时指向 **同一** `.meskeleton`（与 Mesh buddy 同 Guid）；引擎不做 Retarget |
| 7 | 缺轨 | 评价仍 FillBind + 覆盖有轨分量（同 F02） |
| 8 | 过渡混合 | Local Pose TRS：Pos/Scale Lerp，Rot Slerp；`alpha` 由 blendTime 线性（曲线后议） |
| 9 | 编辑器 | MVP：**无**图编辑器；资产可序列化 + Inspector 最小赋 Graph / 调参 |
| 10 | Event | **Out of F03** |

## Reader quick start
1. 本文件：§0 共享层论点 · Locked · §3 数据流 · §4 结构
2. Brief Tier 1（Animator / SM / Params / Blend）；Event 延后
3. F02：`AnimationPlayer` / `AnimationClip::Evaluate` / SMC 兼容检查

---

## 0) 参数基础设施（依赖 CORE-F08）

参数袋实现见 [CORE-F08](../Platform/Core/CORE-F08_PARAMETER_STORAGE_DESIGN.md)。  
代码目录（已锁）：`Runtime/Function/Framework/Parameters/`（非 Animation、非 Core）。

**与 UE 对齐的产品理解：**
- AnimBP ≈ BP **类资产**上的变量生态 —— minEngine **不**走这条路做 Graph 参数。
- Blackboard ≈ **数据资产** + 紧凑实例内存 —— 与 CORE-F08 同构；完整 BB 产品仍未来再做。

本 Feature 只：**声明 Graph 用哪些 ParameterSchemaEntry、过渡如何读 Store、Trigger 如何消费**。

## 1) 背景与目标

### 1.1 现状
- F01：Skeleton / Pose / GPU skinning
- F02：单 Clip + `AnimationPlayer` ⊏ SMC；多 Clip **可**共用同一 Skeleton（Import 时显式选择）
- 无参数袋、无 SM、无 Pose Blend、无 Graph 资产

### 1.2 目标
Gameplay 通过参数驱动状态切换，过渡期视觉连续，输出仍是 **Pose → 既有 palette 路径**。

**成功标准：**
1. 同一 Skeleton 的 Idle/Walk 两 Clip + 一 Graph；`Speed` 跨阈值时过渡混合可目视
2. 参数经 CORE-F08 Store；Graph 仅通过 Store 读参
3. 未赋 Graph 时 F02 单 Clip 路径仍可用

### 1.3 与 Brief 对齐
对应 Brief **Step 5–6**（Pose Blend → Animator+SM）。**Step 7 Event** 不进 F03。到达 Idle/Walk（+可选 Attack 无 Event）后，按 Brief 停止线克制扩展。

---

## 2) 平行产品线

| 层 | F02 | F03 |
|----|-----|-----|
| 共享 | — | **CORE-F08** `ParameterSchema`/`ParameterStore` |
| Asset | AnimationClip | **AnimationGraph**（引用多个 Clip） |
| Runtime | AnimationPlayer | **AnimationGraphInstance** |
| Component | Player 驱动 Pose | Graph **或** Player |
| Render | 无改 | **无改** |

---

## 3) 方案

### 3.1 数据流

```text
Gameplay / Editor / (future Lua)
        │ SetFloat/Bool/Int / SetTrigger
        ▼
 ParameterStore   ◄── CORE-F08
        │
        ▼
 AnimationGraphInstance
   ├─ read transitions (conditions on ParameterStore)
   ├─ advance state / blend alpha
   ├─ ClipA.Evaluate(tA) → PoseA
   ├─ ClipB.Evaluate(tB) → PoseB   (only while blending)
   └─ Pose::Blend(PoseA, PoseB, alpha) → outPose
        │
        ▼
 SkeletalMeshComponent → palette → GPU
```

单 Clip 路径（无 Graph）保持：

```text
AnimationPlayer.Update → Clip.Evaluate → Pose
```

### 3.2 参数（CORE-F08）

Graph 资产嵌入 `ParameterSchema`（或等价 Def 列表）。  
Instance：`ParameterStore::BindLayout(Compile(schema))`。  
过渡条件通过 `KeyId`（或冷路径 name）读 Bool/Int/Float。  
Trigger：Anim 层 `Raise`/`Consume`（建议基于 Bool 槽）。

详见 CORE-F08；此处不重复 Layout 规则。

### 3.3 AnimationGraph 资产

```text
AnimationGraph : Asset
  Schema: ParameterSchema
  States[]:
    Id / Name
    Clip: shared_ptr<AnimationClip>
    bLoop (default true)
  Transitions[]:
    FromState / ToState   // or AnyState later — MVP: explicit from
    Conditions[]:         // AND
      ParamName, Op, Operand  // e.g. Speed > 0.1; Trigger Attack
    BlendDurationSeconds
  DefaultStateId
```

条件运算符 MVP：`>` `>=` `<` `<=` `==` `!=`（Float/Int/Bool）；Trigger 用 `IsSet`。

**校验（Load/Import 时）：**
- 所有 Clip 的 Skeleton Guid 一致（或与可选 Graph.Skeleton 一致）
- 条件引用的 ParamName ∈ Schema
- State Clip 非空

扩展名建议：`.meagraph`（最终以 AssetTypeRegistry 为准）。

### 3.4 AnimationGraphInstance（Runtime）

职责：
- 持有 `ParameterStore`（ResetFromSchema）
- 当前状态、状态内时间、可选「过渡中」：from/to、blend 时钟、两边 Clip 时间
- 每帧：`Update(dt, outPose)`  
  - 非过渡：评价当前 Clip；检查出边（按声明顺序，先匹配先生效）  
  - 过渡中：双评价 + Blend；结束则切到 ToState 并 Consume 相关 Trigger

**不**负责：RHI、Assimp、Gameplay 逻辑。

### 3.5 Pose Blend

```cpp
// Free function or Pose static — prefer Pose::Blend / AnimationPoseUtility::Blend
void BlendPoses(const Pose& a, const Pose& b, float alpha, Pose& out);
```

- `alpha∈[0,1]`；骨数不一致 → 失败或按 min（MVP：要求同骨数，来自同 Skeleton）
- 单测覆盖 0 / 0.5 / 1

### 3.6 SkeletalMeshComponent 接入

```text
if (m_AnimationGraph)
  GraphInstance.Update(dt, m_LocalPose)
else
  AnimationPlayer path (F02)
```

- `EnsureClipSkeletonCompatible` 推广为：Graph 内 Clip 与 Mesh Skeleton Guid 一致
- Inspector：可赋 `AnimationGraph`；可选暴露少量参数调试（Speed）
- PlayOnAwake：有 Graph 则从 DefaultState 播放

### 3.7 与 Player 的关系（再强调）

| | Player | GraphInstance |
|--|--------|---------------|
| 输入 | 单个 Clip | 参数 + Graph 资产 |
| 时间 | 一个时钟 | 每状态一时钟；过渡两个 |
| 输出 | Pose | Pose（可混合） |

Graph **调用** `Clip::Evaluate`，可选用 Player 作为「单 Clip 辅助」但非必须；禁止把 SM 写进 Player。

---

## 4) 测试与 Demo

| 层级 | 内容 |
|------|------|
| 单测 | ParameterStore Set/Get/Trigger consume；Pose Blend；假 Graph Idle↔Walk |
| Editor | 人型：两 Clip 同 Skeleton + Graph；改 Speed 看过渡 |
| 回归 | 无 Graph 时单 Clip 仍播 |

---

## 5) 风险与非目标

| 风险 | 缓解 |
|------|------|
| 共享层做成「半个 Blackboard」膨胀 | Schema 封闭；无 Object；目录不叫 Blackboard |
| 双路径（Player+Graph）永久并存 | Design 写明：Graph 优先；未来可 Deprecated 纯 Player 赋 Clip UX |
| 骨名不完全匹配导致 Clip 残缺 | 文档要求外部重定向 + 同 Skeleton Import；WARN 已有 |
| Trigger 语义扯皮 | Locked：边匹配成功后 Consume |
| 过早图编辑器 | Out；表格式资产 + 最小 Inspector |

---

## 6) 验收标准

- [ ] 参数读写经 **CORE-F08** `ParameterStore`；本模块不实现 Layout 编译
- [ ] `AnimationGraph` Load + Instance 可根据参数切换状态
- [ ] 过渡期 Pose Blend 可目视 / 可单测
- [ ] SMC：Graph 与 单 Clip 路径互斥清晰；同 Skeleton Guid 检查
- [ ] Demo Idle↔Walk PASS
- [ ] Out 清单未偷偷实现（Event / Blend Tree / 完整 Blackboard）
- [ ] Design / Impl / Registry / ACTIVE_WORK / Progress 对齐

---

## 7) 建议切片预览

| Slice | 内容 | 优先级 |
|-------|------|--------|
| S00 | （前置）**CORE-F08** Schema/Layout/Store | **Done**（解阻） |
| S01 | `Pose::Blend`（或等价）+ 单测 | 高 |
| S02 | `AnimationGraph` 资产 schema + Loader/Save | 高 |
| S03 | `AnimationGraphInstance` SM + 过渡混合 | 高 |
| S04 | SMC 接入 + Skeleton Guid 校验 | 高 |
| S05 | Inspector 最小赋 Graph / 调 Speed + 人型 Demo | 高 |
| S06 | （可选）Attack Trigger 状态（仍无 Event） | 中 |

```text
CORE-F08 → S01 Pose Blend → S02 Graph 资产 → S03 Instance → S04 SMC → S05 Demo
                                                    ↘ S06
```

---

## 8) Status note

| 字段 | 内容 |
|------|------|
| Status | **Draft** |
| What's not | Impl Plan；Graph 代码未动；SMC 挂载暂定内嵌 |
| Unblock | 补 ANIM-F03 Impl Plan → Planned → Pre-flight 编码（CORE-F08 已 Done） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry Placeholder：SM + Params + Transition Blend |
| 2026-09-05 | 正式 Design Draft：ParameterStore 共享层；Graph MVP；Event/完整 Blackboard Out |
| 2026-09-05 | 参数实现抽出为 **CORE-F08**；本 Feature 改为依赖 Layout/Store |
| 2026-09-06 | CORE-F08 Done 解阻；Status note / 依赖行同步 |
