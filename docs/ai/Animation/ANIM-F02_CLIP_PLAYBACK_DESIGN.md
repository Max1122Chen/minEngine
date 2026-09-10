# ANIM-F02 — Clip Playback — Design Spec

## Meta
- **ID:** `ANIM-F02`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05（人型 Editor 目视 PASS；OpenGL Int4 bone indices 修复）
- **Branch:** `feat/animation`
- **Related:**
  - [Implementation](./ANIM-F02_CLIP_PLAYBACK_IMPLEMENTATION.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - Prerequisite: [ANIM-F01](./ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md)（**Done**）
  - Follow-up: [ANIM-F03](./ANIM-F03_ANIMATION_GRAPH_DESIGN.md)
- **Depends on:** `ANIM-F01` — **已满足**

## TL;DR
**F02 = MVP 过渡态**（不代表最终 Clip 形态）：Import 写出 `AnimationClip`（**`AnimationTrack` = 骨 TRS**）+ `AnimationPlayer` 组合进 `SkeletalMeshComponent` → `Evaluate` → Pose → F01 蒙皮。  
长期愿景：有限 **named float 轨**；外部用 **`TryGetNamedFloat`** 读采样值，**Clip 不直接改 Component 属性**（详见 §3.5）。

## Scope
- **In:**
  - `AnimationClip`（`.meaclip`）：`AnimationTrack`（BoneIndex + TRS keys）、duration、`shared_ptr<Skeleton>`（序列化走 ObjectPtr 类别，同 `SkeletalMesh`）
  - `TryGetNamedFloat` API 壳 + 可序列化 `NamedFloatTrack` 容器（F02 Import **不**填；恒可返回 false）
  - Assimp → Import 写出引擎 Clip；Load 不跑 Assimp
  - `AnimationPlayer` ⊏ `SkeletalMeshComponent`；单 Clip Play/Pause/Stop/Loop/Speed
  - 单测 + Editor 可 Import AnimationClip（人型 FBX）
- **Out:**
  - Graph / Blend / Event / IK / Root Motion / Retarget
  - Clip 直接写任意 `ME_PROPERTY`；反射绑定表
  - 完整 Anim 编辑器；Clip 压缩

## Locked decisions

| # | 决策 | 说明 |
|---|------|------|
| 1 | Player ⊏ SkeletalMeshComponent | 组合成员 |
| 2 | 缺轨/缺分量 | F02 用 **Bind**；复杂策略后议 |
| 3 | 非 Loop 播完 | **Paused at end** |
| 4 | Clip↔Skeleton | `shared_ptr<Skeleton>` + ObjectPtr 序列化（对齐 SkeletalMesh） |
| 5 | Clip 落盘 | Import 写出 `.meaclip` |
| 6 | Demo | 人型 FBX 动画 |
| 7 | 命名 | `AnimationChannel` → **`AnimationTrack`** |
| 8 | 定位 | F02 = **MVP 过渡**；最终形态另议 |

## Reader quick start
1. 本文件 Locked + §3.2 / §3.5
2. [Impl Plan](./ANIM-F02_CLIP_PLAYBACK_IMPLEMENTATION.md)
3. F01 §2.7 Pose / Skeleton / Component

---

## 1) 背景与目标

```text
AnimationClip.Evaluate(t) → Pose →（F01）palette → GPU
```

成功标准：人型循环播 Clip；Speed/Loop/Pause/Stop 正确；Assimp 不进帧循环。

---

## 2) 平行产品线

| 层 | F01 | F02 |
|----|-----|-----|
| Asset | Skeleton / SkeletalMesh | **AnimationClip** |
| Import | SkeletalMeshLoader | **AnimationClipLoader** |
| Runtime | Pose | Player + Evaluate |
| Component | 写 Pose | Player 驱动 local pose |
| Render | Skinned | **无改** |

---

## 3) 方案

### 3.1 数据流

```text
FBX/glTF (Source) → Assimp (Import only)
  → AnimationClip (.meaclip + meta.SourcePath)
  → AnimationPlayer → Evaluate → Pose → F01
```

### 3.2 数据结构

#### 3.2.1 Key / Track（骨 TRS）

```cpp
ME_STRUCT()
struct AnimationVec3Key { float Time; Vector3 Value; };

ME_STRUCT()
struct AnimationQuatKey { float Time; Quaternion Value; };

ME_STRUCT()
struct AnimationTrack
{
    int32_t BoneIndex = -1;
    std::vector<AnimationVec3Key> PositionKeys;
    std::vector<AnimationQuatKey> RotationKeys;
    std::vector<AnimationVec3Key> ScaleKeys;
};

ME_STRUCT()
struct AnimationNamedFloatKey { float Time; float Value; };

ME_STRUCT()
struct AnimationNamedFloatTrack
{
    std::string Name;
    std::vector<AnimationNamedFloatKey> Keys;
};
```

采样：Pos/Scale 线性；Rot slerp；缺分量 → Bind；Loop 由 Player wrap 时间。

#### 3.2.2 AnimationClip

```cpp
class AnimationClip : public Asset
{
    float GetDuration() const;
    Skeleton* GetSkeleton() const;
    void SetSkeleton(const std::shared_ptr<Skeleton>&);

    const std::vector<AnimationTrack>& GetTracks() const;
    void Evaluate(float timeSeconds, Pose& outPose) const;

    // Future named curves: sample only — never writes Component properties.
    bool TryGetNamedFloat(std::string_view name, float timeSeconds, float& outValue) const;

    std::shared_ptr<Skeleton> m_Skeleton;
    float m_Duration = 0.0f;
    std::vector<AnimationTrack> m_Tracks;
    std::vector<AnimationNamedFloatTrack> m_NamedFloatTracks; // F02: usually empty
};
```

#### 3.2.3 AnimationPlayer

Play / Pause / Stop / Loop / Speed / Time；`Update(dt, outPose)`；非 Loop 越界 → Paused at end。

#### 3.2.4 Component

`AnimationPlayer m_AnimationPlayer` 成员；`Tick` 在 Playing 时 Update → dirty palette。  
可 `ME_PROPERTY` 暴露 `shared_ptr<AnimationClip>` 以便 Inspector 赋值。

### 3.3 导入

| API | 职责 |
|-----|------|
| `AnimationClipLoader::ImportFromFile` | Assimp → tracks；需目标 Skeleton |
| `Save` / `LoadFromAssetMeta` | JSON 序列化 `.meaclip`；Resolve skeleton |
| `AssetManager::ImportAnimationClip` | Sources 复制 + 写出 + Register |

扩展名：`.meaclip`。Editor Import 对话框可选 **AnimationClip**（需同目录 `{stem}_Skeleton.meskeleton`）。

### 3.4 测试与 Demo

单测：中间采样、loop、Paused-at-end、TryGetNamedFloat miss。  
可视：人型 FBX Import Clip → Component 赋值 → Play。

### 3.5 愿景（非 F02 交付）

- Clip = 时间函数集合；骨 TRS 是第一特化。
- **有限 named float 轨**：参数曲线（morph 权重、材质标量等）；调用方 `TryGetNamedFloat` 后**自己**写目标。
- **拒绝（本阶段）**：Clip/Player 直接反射写任意 property（Godot VALUE 全量）。
- F02 留容器 + TryGet 壳，避免日后拆 API。

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| Clip 内嵌 Mesh | 拒绝 |
| 独立 Clip + Player→Pose | **选用** |
| F02 双 Clip Blend | 拒绝 → F03 |
| 独立 Player Component | 拒绝（F02） |
| F02 全量 Property 轨 | Defer → §3.5 |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 骨名不匹配 | Import 警告；Play 校验 Skeleton |
| 坐标系 | 与 F01 同一 Assimp flags / 向量约定 |
| duration=0 | Evaluate no-op；拒 Play |
| MVP 轨模型日后演进 | 文档标明过渡态；`.meaclip` 可再 cook |

---

## 6) 验收

- [x] Import `.meaclip` + Skeleton 绑定
- [x] Evaluate / Player 驱动循环（单测 + 人型目视）
- [x] Loop / Speed / Pause / Stop / Paused-at-end（单测覆盖 loop/pause-at-end；Editor 默认 Loop+PlayOnAwake）
- [x] `TryGetNamedFloat` 对未知名返回 false
- [x] Assimp 不进帧循环；F01 回归（skinned 路径仍用）
- [x] Docs / Registry / Progress 更新

---

## 7) 切片（以 Impl 为准）

| Slice | 目标 |
|-------|------|
| S00 | Track / Clip::Evaluate / TryGet 壳 + 单测 |
| S01 | Loader Import/Save/Load + Registry + AssetManager |
| S02 | Player + SkeletalMeshComponent Tick |
| S03 | Editor Import 选项 + 人型验收入口 |

---

## 8) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done** — MVP 过渡态；人型目视 PASS |
| Next | 下一焦点另议（ANIM-F03 Graph 按需） |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-03 | Draft 初稿 |
| 2026-09-04 | Locked + §3.5 |
| 2026-09-04 | Channel→**Track**；MVP 过渡定位；named float **TryGet** 愿景；Status→In Progress |
| 2026-09-05 | 人型 Walking 目视 PASS；Status→Done；OpenGL Int4 / Guid / ObjectPtr / PlayOnAwake 收尾 |
