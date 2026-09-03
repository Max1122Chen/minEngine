# ANIM-F02 — Clip Playback — Design Spec

## Meta
- **ID:** `ANIM-F02`
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Branch:** `feat/animation`
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - Prerequisite: [ANIM-F01 Skeletal Mesh Pipeline](./ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md)
  - Follow-up: [ANIM-F03 Animation Graph](./ANIM-F03_ANIMATION_GRAPH_DESIGN.md)
  - Brief (reference): [`docs/external/...Development Brief.md`](../../external/minEngine%20—%203D%20Animation%20System%20Development%20Brief.md)
- **Depends on:** `ANIM-F01` 验收（Pose → palette → GPU skinning 正确）

## TL;DR
在 F01 地基上增加 **`AnimationClip` 资产 + 采样 + `AnimationPlayer`**，把「谁写 Pose」从手工/Bind 换成时间轴求值；**不改** SkeletalMesh / Proxy / Material 蒙皮路径。  
与 Static∥Skeletal 平行产品线一致：Clip 是第三条资产线，经 Player 写入 `SkeletalMeshComponent::SetLocalPose`。

## Scope
- **In:**
  - `AnimationClip` Asset（per-bone TRS 曲线、duration）
  - Assimp 动画通道 → Clip（Import 期 bone name→index；绑定目标 `Skeleton`）
  - `AnimationPlayer`：Play / Pause / Stop / Loop / Speed / Time；每帧 `Evaluate → Pose`
  - 与 `SkeletalMeshComponent` 接线（组件持有 Player，或外部求值后 `SetLocalPose`）
  - 单测（采样/循环）+ 可视（Walk/Idle 循环）
- **Out:**
  - State Machine / Animation Graph / Parameters（→ F03）
  - Animation Event、Root Motion、IK、Retarget、Additive、Layer、Blend Tree
  - 多 Clip 混合（F03 Transition Blend；本 Feature 最多 **单 Clip**）
  - Clip 压缩、流式、Job 系统
  - 完整 Anim 编辑器

## Reader quick start
1. 本文件：Clip / Player 契约及与 F01 的接缝
2. F01 §2.7：`Pose` / `Skeleton` / `SkeletalMeshComponent` 接口
3. 实现计划：待 `ANIM-F02_CLIP_PLAYBACK_IMPLEMENTATION.md`

---

## 1) 背景与目标

F01 交付后，系统已能：

```text
Pose → Skeleton::BuildSkinningPalette → SkeletalMeshSceneProxy → Skinned VS
```

缺的是 **随时间变化的 Pose 源**。F02 只补这一环：

```text
AnimationClip.Evaluate(time) → Pose →（F01 管道）
```

**成功标准：** 导入角色循环播放至少一条有意义的 Clip（如 Walk）；改 Speed/Loop 行为正确；Assimp 仍不进帧循环。

---

## 2) 与 F01 平行产品线的关系

| 层 | F01 | F02 增量 |
|----|-----|----------|
| Asset | `Skeleton` / `SkeletalMesh` | **`AnimationClip`**（引用兼容的 `Skeleton`） |
| Import | `SkeletalMeshLoader` | **`AnimationClipLoader`**（可同文件抽 anim，或独立入口） |
| Runtime 核 | `Pose` / palette | **采样器 + `AnimationPlayer`** |
| Component | `SkeletalMeshComponent` 写 Pose | Player 驱动 `SetLocalPose` |
| Render / Material | Skinned 路径 | **无改动**（验收项：零回归蒙皮） |

F03 将再把「Pose 源」换成 Graph；Player 可作为 Graph 内单状态的实现，或被 Graph 替代为唯一入口——**F02 API 保持小而稳**，避免 Graph 倒逼重写采样器。

---

## 3) 方案

### 3.1 数据流

```text
FBX/glTF animation
        ↓ Assimp（仅 Import）
AnimationClip (channels by bone index)
        ↓
AnimationPlayer (time, speed, loop)
        ↓ Evaluate
Pose (local TRS)
        ↓
SkeletalMeshComponent::SetLocalPose
        ↓
（F01）palette → GPU
```

### 3.2 数据结构与接口

#### 3.2.1 关键帧与通道

```cpp
template<typename T>
struct AnimationKey
{
    float Time = 0.0f; // 秒，相对 Clip 起点
    T Value{};
};

struct AnimationChannel
{
    int32_t BoneIndex = -1; // 相对 Clip 绑定的 Skeleton；Import 后禁止靠名字

    std::vector<AnimationKey<Vector3>> PositionKeys;
    std::vector<AnimationKey<Quaternion>> RotationKeys;
    std::vector<AnimationKey<Vector3>> ScaleKeys;
};
```

**采样规则（契约）：**
- Position / Scale：线性插值  
- Rotation：slerp（四元数已归一化）  
- 时间在两端之外：Clamp（非 Loop 时）；Loop 由 Player 对 time 取模后再采样  
- 某通道缺 Position/Rotation/Scale：**回退到 Skeleton bind local 对应分量**（或 Identity 分量）——实现选一种并单测钉死；推荐 **缺省用 Bind 的该分量**，避免 T-pose 崩坏  

#### 3.2.2 AnimationClip 资产

```cpp
class AnimationClip : public Asset
{
public:
    float GetDuration() const;          // 秒；通常 = max key time
    Skeleton* GetSkeleton() const;      // 兼容骨架（逻辑绑定）
    void SetSkeleton(const std::shared_ptr<Skeleton>& skeleton);

    const std::vector<AnimationChannel>& GetChannels() const;

    // 写入 outPose（须已按 boneCount Resize）；未覆盖的骨保持 outPose 原值或先 FillBindPose
    void Evaluate(float timeSeconds, Pose& outPose) const;

private:
    std::shared_ptr<Skeleton> m_Skeleton;
    float m_Duration = 0.0f;
    std::vector<AnimationChannel> m_Channels;
};
```

**Skeleton 兼容（F02）：**
- Clip 必须绑定与目标 `SkeletalMesh` **同一** `Skeleton` 资产（指针/GUID 相等），或 Import 时按名字映射到该 Skeleton 的 index。  
- **不做** Runtime Retarget；不匹配则拒绝 Play 并打错误日志。

#### 3.2.3 AnimationPlayer

```cpp
enum class AnimationPlayState : uint8_t
{
    Stopped,
    Playing,
    Paused,
};

class AnimationPlayer
{
public:
    void SetClip(const std::shared_ptr<AnimationClip>& clip);
    AnimationClip* GetClip() const;

    void Play();
    void Pause();
    void Stop();                 // time → 0，State → Stopped

    void SetLooping(bool loop);
    bool IsLooping() const;

    void SetSpeed(float speed);  // 可负（倒放）；默认 1
    float GetSpeed() const;

    void SetTime(float timeSeconds);
    float GetTime() const;

    AnimationPlayState GetState() const;

    // dt 推进时间；写入 outPose。Stopped 时可不改 pose 或保持最后一帧——推荐 Stopped 不调用 Evaluate
    void Update(float deltaSeconds, Pose& outPose);

private:
    std::shared_ptr<AnimationClip> m_Clip;
    float m_Time = 0.0f;
    float m_Speed = 1.0f;
    bool m_bLooping = true;
    AnimationPlayState m_State = AnimationPlayState::Stopped;
};
```

**时间推进：**
- `Playing`：`m_Time += delta * m_Speed`  
- `Looping`：在 `[0, duration)` 上 wrap（duration≤0 则 no-op）  
- 非 Loop 且越界：Clamp 到端点并 → `Paused` 或 `Stopped`（推荐 **Paused at end**，便于 UI；实现计划钉死）

#### 3.2.4 与 Component 接线

**推荐默认（F02 MVP）：**

```cpp
// SkeletalMeshComponent 增量（F02）
void SetAnimationPlayer(std::unique_ptr<AnimationPlayer> player); // 或内嵌成员
AnimationPlayer* GetAnimationPlayer();

// 在 Component Tick / Scene 更新中：
// if (player && playing) { player->Update(dt, m_LocalPose); m_bPoseDirty = true; }
```

备选：独立 `AnimationPlayerComponent` 找同 Entity 的 `SkeletalMeshComponent` 写 Pose——更「组合」，但多一层查找；**F02 默认内嵌/附属在 SkeletalMeshComponent**，F03 Graph 再决定是否外置。

**不变量：** Player **从不**碰 RHI / Material / Assimp；只产出 `Pose`。

### 3.3 导入管线

```text
Assimp scene.mAnimations[i]
  → 通道 mNodeName → Skeleton.FindBoneIndex
  → Position/Rotation/Scale keys（坐标与 F01 网格导入同一套约定）
  → AnimationClip
```

| API | 职责 |
|-----|------|
| `AnimationClipLoader::ImportFromFile(path, skeleton, outClip)` | 指定目标 Skeleton；映射通道 |
| `LoadFromAssetMeta` | meta + 源文件；需能解析到 Skeleton 引用（meta 字段或同目录约定） |

**与网格同 FBX：**  
允许一次源文件分别导入 `SkeletalMesh` 与 `AnimationClip`（两次 Import 或扩展 Import UX）。Clip 不强制与 Mesh 同 meta。

**AssetTypeRegistry：** `AnimationClip`。

### 3.4 测试与 Demo

| 层级 | 内容 |
|------|------|
| 单测 | 两关键帧中间采样；duration 边界；loop wrap；缺通道回退 Bind |
| 可视 | 角色循环 Walk；改 Speed；Pause/Stop |
| 回归 | F01 Bind/单骨调试仍可用；StaticMesh smoke |

---

## 4) 备选方案

| 选项 | 说明 | 结论 |
|------|------|------|
| A. Clip 内嵌在 SkeletalMesh | 少资产，无法多动画共享骨架 | **拒绝** |
| B. 独立 `AnimationClip` + Player → Pose | 与 F01/F03 接缝干净 | **选用** |
| C. F02 做双 Clip Blend | 抢 F03 范围 | **拒绝** |
| D. 独立 Player Component | 更组合，F02 多样板代码 | **Defer**；MVP 挂在 SkeletalMeshComponent |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 通道名与 Skeleton 对不上 | 部分骨不动 / 错绑 | Import 报告未匹配通道；Play 前校验 |
| 坐标系与网格导入不一致 | 滑动、翻转 | 与 F01 同一 Assimp 后处理 |
| duration=0 / 空 Clip | 除零、闪烁 | Evaluate no-op；Player 拒绝 Play |
| 负 Speed + Loop | 边界 wrap 易错 | 单测覆盖 |

---

## 6) 验收标准

- [ ] `AnimationClip` 可导入并绑定 `Skeleton`
- [ ] `Evaluate` / `AnimationPlayer` 驱动角色循环动画（可视）
- [ ] Loop / Speed / Pause / Stop 行为符合 §3.2.3
- [ ] 蒙皮路径无分叉；F01 回归通过
- [ ] Assimp 不参与每帧更新
- [ ] Design / Registry / Progress 更新；Impl Plan 开工前就绪

---

## 7) 建议切片（预览）

| Slice | 目标 |
|-------|------|
| S00 | `AnimationKey` / `Channel` / `Clip::Evaluate` + 单测 |
| S01 | Assimp → `AnimationClipLoader` + AssetType |
| S02 | `AnimationPlayer` + `SkeletalMeshComponent` 接线 |
| S03 | Demo 资产 + 可视验收 |

---

## 8) Status note

| 字段 | 内容 |
|------|------|
| Status | **Draft** — 可与 F01 并行审阅；**实现**排在 F01 之后 |
| Unblock 实现 | F01 Pose→GPU 验收 |
| Next | 审阅 → 等 F01 Impl；再写 F02 Implementation Plan |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Registry 占位 |
| 2026-09-03 | Draft：Clip/Player 数据结构与接口；对齐 F01 平行产品线与 Pose 接缝 |
