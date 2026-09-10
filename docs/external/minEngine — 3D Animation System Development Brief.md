# minEngine — 3D Animation System Development Brief

## 0. Agent Role

你现在参与开发的是一个个人 C++ 游戏引擎项目 **minEngine**。

请将自己视为：

> **Engine Architecture / Technical Lead + Implementation Agent**

而不是单纯的代码生成器。

在实现任何 Animation System 功能之前，优先考虑：

1. 架构边界是否合理
2. 数据所有权是否清晰
3. Runtime / Asset / Rendering / Gameplay 是否解耦
4. 是否符合 minEngine 当前阶段的实际需求
5. 是否会引入不必要的长期复杂度
6. 是否值得当前投入
7. 是否能够形成一个真正可用的 Game Runtime

**不要为了“功能完整”而复制 Unreal Engine / Unity 的全部 Animation Framework。**

minEngine 的目标不是成为 miniature Unreal Engine。

---

# 1. minEngine 的整体设计哲学

minEngine 是一个用于：

- 学习游戏引擎开发
- 展示 Engine Architecture 能力
- 求职
- 最终真正制作并发布一个小游戏

的个人 C++ 游戏引擎。

当前已有：

- C++
- OpenGL + GLFW + GLM
- Vulkan RHI 正在完善
- Renderer / RHI abstraction
- Forward Rendering
- PBR
- Directional Shadow / CSM / PCF
- FXAA / Sharpen
- Physics
- Reflection / Header Tool
- JSON Serialization
- Lua
- Delegate / Event
- Scene / Entity / Component
- Assimp
- ImGui / Developer Tools

整体架构强调：

> **上层依赖稳定的抽象能力，而不是依赖具体底层实现。**

类似 TCP/IP 的分层思想：

```text
Gameplay
    ↓
Engine Service / Interface
    ↓
Subsystem
    ↓
RHI / Platform
```

底层实现可以替换，而不应该迫使上层发生大规模修改。

Animation System 必须遵循这一思想。

---

# 2. 当前 Animation System 的战略目标

当前目标不是：

> 实现一个工业级动画引擎。

而是：

> **实现一个足以支撑真实 3D 游戏 Demo，同时能够体现现代游戏引擎动画系统设计能力的轻量级 Skeletal Animation Runtime。**

最终必须能够形成完整闭环：

```text
Source Asset
    ↓
Asset Importer
    ↓
Skeleton / Skinned Mesh / Animation Clip
    ↓
Animation Runtime
    ↓
Pose
    ↓
Bone Matrices
    ↓
GPU Skinning
    ↓
Renderer
```

并且进一步：

```text
Gameplay
    ↓
Animator Parameters
    ↓
State Machine
    ↓
Animation
    ↓
Animation Event
    ↓
Gameplay / Physics / Audio
```

例如：

```text
Attack
  ↓
Animation Event
  ↓
Enable Hitbox
  ↓
Physics Collision
  ↓
Damage
  ↓
Audio
```

这比单纯“播放 FBX 动画”更加重要。

---

# 3. Animation System 的核心架构思想

不要把所有动画能力看成一个系统。

应该理解为多个层：

```text
Layer 4 — Gameplay Integration

    Animation Event
    Root Motion
    Gameplay Interaction
    Hitbox
    Audio
        ↓

Layer 3 — Animation Control

    Animator
    State Machine
    State
    Transition
    Blend
    Parameters
        ↓

Layer 2 — Animation Evaluation

    Animation Clip
    Pose
    Skeleton
    Bone Transform
        ↓

Layer 1 — Deformation / Rendering

    Skinning
    Bone Matrix
    GPU Buffer
    Vertex Shader
```

核心原则：

> **Animation Runtime 尽可能是一个“Pose Generator / Pose Evaluator”。**

输入：

```text
Time
Parameters
Animation State
```

输出：

```text
Pose
Animation Events
(optional) Root Motion
```

不要让 Animation System 直接负责：

```text
Physics
Audio
Damage
Entity Gameplay
```

这些应该通过 Service / Event / Interface 进行交互。

---

# 4. 最核心的 Runtime 数据流

Animation System 最重要的数据流应该是：

```text
AnimationClip
      ↓
Evaluate(time)
      ↓
Pose
      ↓
Skeleton Hierarchy Evaluation
      ↓
Global Bone Transforms
      ↓
Inverse Bind Pose
      ↓
Skinning Palette
      ↓
GPU
```

其中：

```text
Pose
```

是非常重要的中间抽象。

推荐：

```cpp
struct Pose
{
    vector<Transform> localTransforms;
};
```

Animation Clip 不应该直接操作 Mesh。

Animation Clip 负责产生 Pose。

Skeleton 负责描述：

- Bone
- Parent
- Hierarchy
- Bind Pose
- Bone Identity

Renderer 负责：

- Bone Matrix Buffer
- GPU Skinning
- Graphics API interaction

---

# 5. Skeleton

Skeleton 是 Animation System 的基础。

概念上：

```text
Skeleton
 └── Bone hierarchy

Root
├── Spine
│   ├── Chest
│   │   ├── Arm_L
│   │   └── Arm_R
│   └── Head
├── Leg_L
└── Leg_R
```

Bone 至少需要表达：

```text
Bone Index
Parent Index
Name / Identity
Local Bind Transform
Inverse Bind Matrix
```

注意：

> Runtime 不应该在每帧通过字符串查找 Bone。

Import 阶段应完成：

```text
Bone Name
    ↓
Bone Index
```

Runtime 使用：

```text
Bone Index
```

---

# 6. Local / Global / Inverse Bind Pose

必须明确理解三个概念：

## Local Transform

Bone 相对于 Parent 的 Transform。

```cpp
global =
    parentGlobal * local;
```

## Global Transform

从 Root 累积到当前 Bone 的 Transform。

## Inverse Bind Pose

描述 Bind Pose 下将 Mesh 从 Model Space 转换到 Bone Space 所需的逆变换。

最终：

```cpp
SkinMatrix =
    CurrentGlobalTransform *
    InverseBindMatrix;
```

这组数学关系是 GPU Skinning 的基础。

---

# 7. Skinned Mesh

Skinned Mesh 与 Static Mesh 的区别主要在于：

```text
Vertex
 ├── Position
 ├── Normal
 ├── UV
 ├── Bone Indices
 └── Bone Weights
```

典型：

```cpp
struct SkinnedVertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;

    Vec4 boneWeights;
    UVec4 boneIndices;
};
```

例如：

```text
Vertex
 ├── Bone 3 : 0.7
 ├── Bone 7 : 0.2
 ├── Bone 8 : 0.1
 └── Bone 0 : 0.0
```

第一版只需要支持合理数量的骨骼影响，例如 4 influences。

不要过度设计无限骨骼影响。

---

# 8. GPU Skinning

第一版正式 Runtime 应优先采用：

> **GPU Skinning**

CPU 每帧主要负责：

```text
Animation Evaluation
        ↓
Pose
        ↓
Bone Global Transforms
        ↓
Skinning Matrices
```

然后上传：

```text
BoneMatrices[]
```

GPU Vertex Shader：

```text
weights.x * boneMatrices[indices.x]
+
weights.y * boneMatrices[indices.y]
+
weights.z * boneMatrices[indices.z]
+
weights.w * boneMatrices[indices.w]
```

然后完成 Vertex Deformation。

CPU Skinning 可以作为：

- Debug Reference
- 测试实现
- 验证数学正确性的工具

但不应该成为主要 Runtime Rendering Pipeline。

---

# 9. OpenGL / Vulkan 抽象

Animation System 不应该知道：

```text
GLuint
VkBuffer
VkDescriptorSet
```

Animation Runtime 只产生：

```text
Skinning Palette
```

Renderer / RHI 负责：

```text
Animation
    ↓
Bone Matrix Data
    ↓
Renderer
    ↓
RHI
 ┌──┴─────┐
OpenGL   Vulkan
```

原则：

> Animation System 不依赖 Graphics API。

Renderer 负责决定：

- UBO / SSBO / Buffer
- Descriptor
- Binding
- Upload
- Shader resource layout

Animation Runtime 只表达：

```text
“这里有 N 个 Bone Matrices。”
```

---

# 10. Animation Clip

Animation Clip 本质：

> **随时间变化的 Bone Transform Channels。**

例如：

```text
Walk
duration = 1.2 sec

Arm_L rotation
    t=0.0
    t=0.3
    t=0.6
    t=0.9
    t=1.2
```

典型结构：

```cpp
AnimationChannel
{
    boneIndex;

    positionKeys;
    rotationKeys;
    scaleKeys;
}
```

插值：

```text
Position → Lerp
Rotation → Slerp / normalized interpolation
Scale    → Lerp
```

第一版不需要做：

- Advanced curve systems
- Compression
- Quantization
- Sophisticated curve reduction

先保证正确、清晰、可维护。

---

# 11. AnimationClip 与 Skeleton 的关系

推荐：

```text
Skeleton
    ↓
Bone Hierarchy

AnimationClip
    ↓
Bone Animation Channels
```

Animation Clip 不应该拥有 Skeleton。

Importer 阶段建立：

```text
Bone Name → Bone Index
```

Runtime 使用：

```text
Bone Index
```

这样避免：

```text
per-frame string lookup
```

并允许多个 Animation Clip 作用于同一个 Skeleton。

---

# 12. Source Asset 与 Runtime Asset

推荐明确区分：

```text
FBX / GLTF
    ↓
Assimp
    ↓
Importer
    ↓
Cook / Convert
    ↓
Engine Runtime Asset
```

不要让 Runtime 每次启动都重新解析 FBX。

例如：

```text
Source:
    Knight.fbx

Runtime:
    Knight.mesh
    Knight.skeleton
    Knight.anim
```

或者未来统一：

```text
Knight.asset
```

Asset Pipeline 应该逐步向：

```text
Source Asset
    ↓
Import
    ↓
Engine Asset
    ↓
Runtime
```

演进。

---

# 13. Skeleton / Mesh / Animation Asset Ownership

推荐概念关系：

```text
Skeleton
   ↑
   │
SkinnedMesh
```

而不是让 Mesh 永久拥有 Skeleton。

因为未来可能出现：

```text
Skeleton
   ↓
Animation Clips

SkinnedMesh A
SkinnedMesh B
   ↓
same Skeleton
```

或者多个角色共享相同 Skeleton / Animation。

第一版不必实现复杂共享机制，但数据模型不要把自己锁死。

---

# 14. AnimationPlayer

AnimationPlayer 解决：

> “播放哪个 Clip，以及时间如何推进？”

负责：

```text
Current Clip
Time
Speed
Loop
Playing
Pause
```

例如：

```cpp
player.Play("Walk");
```

每帧：

```text
Update(deltaTime)
    ↓
time += deltaTime
    ↓
AnimationClip::Evaluate(time)
    ↓
Pose
```

AnimationPlayer 的职责应该保持简单。

不要把 Gameplay State Machine 塞进 Player。

---

# 15. AnimationPlayer 与 Animator 的职责区别

这是一个非常重要的架构边界：

## AnimationPlayer

> **播放一个动画。**

例如：

```text
Walk
time = 0.35
speed = 1.0
```

## Animator

> **决定现在应该播放什么动画。**

例如：

```text
Speed = 4.2
Grounded = true
Attack = false

        ↓

Walk
```

因此：

```text
Animator
    ↓
AnimationPlayer
    ↓
AnimationClip
    ↓
Pose
```

Animator 不应该取代 Player。

---

# 16. Animation State Machine

第一版应该实现一个轻量 State Machine。

例如：

```text
Idle
 │
 │ Speed > 0.1
 ▼
Walk
 │
 │ Speed > 3.0
 ▼
Run
```

以及：

```text
Idle
  ↓
Attack
  ↓
Idle
```

核心数据：

```text
AnimationState
AnimationTransition
Condition
Blend Duration
```

State Machine 是 Tier 1 的核心能力。

---

# 17. Animation Parameters

不要让 State Machine 直接绑定 Gameplay C++ 变量。

推荐：

```cpp
animator.SetFloat("Speed", speed);
animator.SetBool("Grounded", grounded);
animator.SetTrigger("Attack");
```

State Machine 根据参数判断 Transition：

```text
Speed > 0.1
```

Gameplay：

```text
Player
    ↓
Animator Parameters
    ↓
State Machine
```

Lua 也可以通过同一接口：

```lua
animator:SetFloat("Speed", speed)
animator:SetTrigger("Attack")
```

这样 Gameplay 与 Animation Control 之间形成稳定 Interface。

---

# 18. Pose Blend

Blend 是第一版非常值得做的能力。

例如：

```text
Idle Pose
      \
       Blend(alpha)
      /
Walk Pose
      ↓
Final Pose
```

Transform：

```text
Position → Lerp
Rotation → Slerp
Scale    → Lerp
```

Blend 的收益非常高，而实现复杂度相对低。

因此：

> **强烈推荐作为 MVP 后期能力。**

---

# 19. Animation Event

Animation Event 是 Animation 与 Gameplay 之间非常重要的桥梁。

例如：

```text
Attack Animation

0.00 ─────────────── 1.00
        ↑       ↑
     HitStart  HitEnd
```

数据：

```cpp
AnimationEvent
{
    float time;
    EventId / Name;
}
```

Runtime：

```text
Animation Evaluation
        ↓
Event reached
        ↓
Delegate / Event
        ↓
Gameplay
```

例如：

```text
Attack
 ↓
EnableHitbox
 ↓
Physics
 ↓
Damage
```

或者：

```text
Footstep
 ↓
Audio
```

不要让 Animation System 直接调用 Physics / Audio。

使用 Event / Service Interface。

---

# 20. Animation 与 Gameplay 的目标关系

最终希望形成：

```text
Input
  ↓
Gameplay
  ↓
Animator Parameter
  ↓
State Machine
  ↓
Animation
  ↓
Animation Event
  ↓
Gameplay / Physics / Audio
```

Animation 是 Runtime 中的一环，而不是孤立 Demo。

例如：

```text
Player Input
    ↓
Attack
    ↓
Animator.Trigger("Attack")
    ↓
Attack State
    ↓
Attack Clip
    ↓
Animation Event
    ↓
Enable Hitbox
    ↓
Physics
    ↓
Damage
```

这是最终 Demo 非常值得展示的 Pipeline。

---

# 21. Tier System

## Tier 0 — 必须实现

这些组成真正可用的 Skeletal Animation MVP：

```text
✓ Skeleton
✓ Bone
✓ Bone Hierarchy
✓ Pose
✓ Skinned Mesh
✓ Bone Weights
✓ Bone Indices
✓ Inverse Bind Pose
✓ Animation Clip
✓ Keyframe Evaluation
✓ Animation Player
✓ GPU Skinning
```

目标：

> 能导入一个角色，并让角色播放真实 3D 动画。

---

## Tier 1 — 强烈值得做

```text
✓ Animator
✓ Animation State
✓ Transition
✓ State Machine
✓ Parameters
✓ Pose Blend
✓ Transition Blend
✓ Animation Event
```

目标：

> Animation 从“播放器”升级为真正的 Game Runtime Animation System。

---

## Tier 2 — 后续能力

根据 Demo 需求选择：

```text
Blend Tree
Root Motion
Animation Layer
Additive Animation
Animation Mask
```

这些不是 MVP 的必要条件。

---

## Tier 3 — 当前不要做

明确禁止当前阶段陷入：

```text
IK
Retargeting
Complex Rigging
Montage System
Animation Compression
Advanced Curve Compression
Pose Cache
Animation Job System
Animation LOD
Pose Sharing
复杂 Runtime Retargeting
```

原因：

> 这些能力虽然属于现代工业级 Animation System，但与当前 minEngine 的核心目标相比，复杂度 / 收益比过低。

如果 Demo 明确需要其中某一项，再单独评估。

---

# 22. 复杂度爆炸点

Animation System 的复杂度不是线性的。

基础：

```text
Clip
 ↓
Pose
 ↓
Skeleton
 ↓
Skinning
```

非常简单。

加入 Player：

```text
time
 ↓
Player
 ↓
Clip
 ↓
Pose
```

仍然简单。

加入 Blend：

```text
Clip A ─┐
        ├─ Blend → Pose
Clip B ─┘
```

仍然可控。

加入 State Machine：

```text
Gameplay
 ↓
Parameters
 ↓
State Machine
 ├── Idle
 ├── Walk
 └── Attack
 ↓
Clip
 ↓
Pose
```

开始产生：

```text
State ownership
Transition
Interrupt
Timing
Parameter dependency
```

加入 Blend Tree：

```text
State
 ↓
Blend Tree
 ├── Idle
 ├── Walk
 └── Run
```

State 不再对应单个 Clip。

加入 Layer：

```text
Base Layer
Upper Body Layer
        ↓
Pose Combination
```

开始出现：

```text
Mask
Weight
Override
Additive
Priority
```

加入 IK：

```text
Animation
 ↓
Blend
 ↓
Layer
 ↓
IK
 ↓
Final Pose
```

开始依赖 World / Physics。

加入 Root Motion：

```text
Animation
 ↓
Root Motion
 ↓
Character Controller
 ↓
Physics
```

Animation 开始反向影响 Gameplay。

因此：

> **真正的复杂度来源是系统之间的组合，而不是功能数量本身。**

---

# 23. Root Motion

第一版默认：

> **不做。**

除非当前 Game Demo 明确依赖 Root Motion。

原因：

Root Motion 会涉及：

```text
Animation
 ↓
Character Transform
 ↓
Character Controller
 ↓
Physics
```

从而产生：

```text
Who owns Transform?
Who moves Character?
Animation or Physics?
How are collisions handled?
```

这是一个架构级问题。

如果未来需要，再设计：

```text
Animation Runtime
    ↓
Root Motion Delta
    ↓
Character Controller
```

而不是 Animation System 直接修改 Transform。

---

# 24. IK

当前阶段：

> **不要做。**

不要因为“现代引擎都有 IK”就实现。

IK 很容易扩张成：

```text
Two Bone IK
Foot IK
Look At
CCD
FABRIK
Pole Vector
Constraints
IK Pass
```

这会变成独立项目。

如果 Demo 不需要：

> 不做。

---

# 25. Retargeting

当前阶段：

> **不要做。**

它会引入：

```text
Skeleton A
 ↓
Bone Mapping
 ↓
Skeleton B
 ↓
Proportion Conversion
 ↓
Pose Conversion
```

同时需要处理：

```text
Different hierarchy
Different proportions
Different orientation
Different rest pose
```

第一版直接要求 Demo 使用兼容 Skeleton。

一个完整的 Animation Runtime 比半吊子的 Retargeting 更有价值。

---

# 26. Animation Layer

当前阶段：

> Tier 2。

它适用于：

```text
Lower Body
    Walk

Upper Body
    Shoot
```

但会引入：

```text
Layer
Mask
Weight
Override
Additive
Sync
```

等复杂度。

Demo 明确需要时再做。

---

# 27. Blend Tree

当前阶段：

> Tier 2。

虽然很有用：

```text
Speed
 ↓
Idle / Walk / Run
```

但 MVP 完全可以用：

```text
Speed < 0.1 → Idle
Speed < 3.0 → Walk
Speed >= 3.0 → Run
```

配合 Transition Blend 实现。

不要为了“像 Unity / UE”而提前实现 Blend Tree。

---

# 28. Animation Compression

当前阶段：

> 不做。

先保存原始 Keyframes。

不要提前实现：

```text
Quaternion compression
Quantization
Key reduction
Curve compression
```

除非真正遇到 Asset Size / Runtime Memory 问题。

---

# 29. Pose Cache / Animation Jobs / Animation LOD

当前阶段：

> 不做。

这些属于规模优化。

如果以后出现：

```text
1000+ Characters
```

再考虑：

```text
Pose Cache
Job System
Animation LOD
Pose Sharing
```

当前小游戏 Demo 不需要。

---

# 30. Component Integration

推荐 Runtime 关系：

```text
Entity
 │
 ├── TransformComponent
 │
 ├── SkeletalMeshComponent
 │       ├── Mesh
 │       └── Skeleton
 │
 └── AnimatorComponent
         ├── Parameters
         ├── StateMachine
         ├── CurrentState
         └── Pose
```

数据流：

```text
AnimatorComponent
        ↓
Animation Runtime
        ↓
Pose
        ↓
SkeletalMeshComponent
        ↓
Renderer
```

具体命名可以根据 minEngine 当前 Component 架构调整。

---

# 31. Animation System 与 Rendering 的边界

推荐：

```text
Animation Runtime
    ↓
Pose / Skinning Palette
    ↓
Skeletal Mesh Renderer
    ↓
Renderer
    ↓
RHI
```

Animation 不应该知道：

```text
RenderPass
Pipeline
OpenGL
Vulkan
Descriptor
```

Renderer 不应该负责：

```text
State Machine
Animation Parameters
Clip Evaluation
```

两者通过：

```text
Pose / Skinning Data
```

进行交互。

---

# 32. Asset Pipeline 与 Assimp

当前使用 Assimp。

推荐：

```text
FBX / GLTF
    ↓
Assimp
    ↓
Animation Importer
    ↓
┌──────────────┬───────────────┬───────────────┐
│              │               │
Mesh         Skeleton       Animation
│              │               │
└──────────────┴───────────────┘
               ↓
        Engine Runtime Assets
```

Importer 应负责：

- Bone discovery
- Bone hierarchy
- Bone index assignment
- Inverse bind pose extraction
- Animation channel mapping
- Keyframe conversion
- Coordinate conversion
- Asset validation

Runtime 不应该依赖 Assimp。

> **Assimp 是 Import Tool / Pipeline dependency，不应该成为 Animation Runtime dependency。**

---

# 33. Runtime Asset 与 Source Asset

设计目标：

```text
Source
    ↓
Import
    ↓
Cook
    ↓
Runtime Asset
```

Runtime 不应保存不必要的：

```text
Assimp structures
Source format details
FBX-specific assumptions
```

未来应该能够：

```text
FBX
GLTF
Other formats
```

统一转换为 minEngine Runtime Asset。

---

# 34. 推荐开发路线

## Step 1 — Skeleton

实现：

```text
Bone
Skeleton
Hierarchy
Bind Transform
Inverse Bind Pose
```

完成后：

> minEngine 可以表达一个骨骼结构。

---

## Step 2 — Skinned Mesh + GPU Skinning

实现：

```text
Bone Indices
Bone Weights
Bone Matrices
Shader Skinning
```

完成后：

> minEngine 可以渲染真正的骨骼角色。

首先做：

```text
T-Pose
→
人工构造 Pose
→
Skinning
```

确认 Skinning 数学正确。

不要一开始就把所有系统接起来。

---

## Step 3 — Animation Clip

实现：

```text
Position Keys
Rotation Keys
Scale Keys
Interpolation
```

完成后：

> 一个 Animation Clip 可以生成 Pose。

---

## Step 4 — Animation Player

实现：

```text
Play
Stop
Pause
Loop
Speed
Time
```

完成后：

> minEngine 可以真正播放 Walk / Run / Idle。

---

## Step 5 — Pose Blend

实现：

```text
Pose A
Pose B
Alpha
↓
Blended Pose
```

完成后：

> Animation Transition 不再发生视觉跳变。

---

## Step 6 — Animator + State Machine

实现：

```text
State
Transition
Parameter
Condition
Blend
```

完成后：

> Animation 开始根据 Gameplay 状态自动运行。

---

## Step 7 — Animation Event

实现：

```text
Animation Timeline
 ↓
Event
 ↓
Delegate / Gameplay Event
```

完成后：

> Animation 可以真正驱动 Gameplay。

例如：

```text
Attack
 ↓
Hitbox
 ↓
Physics
 ↓
Damage
```

---

## Step 8 — Real Game Integration

此时不要继续扩展 Animation Feature。

直接实现：

```text
Player
Enemy
Idle
Walk
Run
Attack
Hit
Death
Footstep
Audio
```

然后将 Animation 与：

```text
Gameplay
Physics
Audio
UI
```

整合。

---

# 35. 最重要的“停止线”

当以下能力全部完成：

```text
✓ Skeleton
✓ Skinned Mesh
✓ GPU Skinning
✓ Animation Clip
✓ Animation Player
✓ Pose
✓ Blend
✓ Animator
✓ State Machine
✓ Parameters
✓ Animation Event
```

并且可以实现：

```text
Idle
Walk
Run
Attack
Hit
Death
```

时：

> **停止继续开发 Animation System。**

不要自动进入：

```text
Blend Tree
→ Layer
→ IK
→ Retargeting
→ Montage
→ Compression
→ Animation Jobs
→ LOD
```

除非实际 Game Demo 对某个功能有明确需求。

---

# 36. 判断一个新 Animation Feature 是否应该开发

任何 Agent 提出新功能时，先回答：

### Question 1

这个功能是否是当前 Game Demo 的实际需求？

### Question 2

它能否显著提高 Game Runtime 的完整度？

### Question 3

实现它是否会引入新的长期架构依赖？

### Question 4

是否可以用已有能力以更简单的方式解决？

### Question 5

它是 Core Runtime，还是 Industrial-scale Optimization？

如果：

```text
需求低
复杂度高
```

→ **不要做。**

如果：

```text
需求高
复杂度低
```

→ **优先做。**

---

# 37. 推荐参考对象

不要机械复制任何一个引擎。

参考对象应该分别用于学习不同层次。

## Unity

重点观察：

```text
Animator
AnimationClip
Animator Parameters
State Machine
Blend Tree
Animation Event
```

适合学习：

> **Data-driven Animation Authoring Model**

但不要照搬 Unity 的 Editor / Runtime 体系。

---

## Unreal Engine

重点观察：

```text
USkeleton
UAnimSequence
UAnimInstance
Animation Blueprint
State Machine
Blend
Pose
Montage
Slot
Layer
```

适合学习：

> **大型 Game Runtime 中 Animation 与 Gameplay 的结合方式**

尤其关注：

```text
Animation Blueprint
=
Animation Logic + State + Parameters
```

但不要复制 UE 的巨大 Gameplay Framework。

---

## Godot

适合观察：

```text
AnimationPlayer
AnimationTree
Skeleton3D
Skin
```

重点理解：

> **如何用相对轻量的架构提供完整 Animation 能力。**

这与 minEngine 的“轻量 Game Runtime”目标比较接近。

---

## glTF

重点参考：

```text
Skin
Skeleton / Joints
Inverse Bind Matrices
Animation Channels
```

适合作为：

> Runtime Asset / Interchange Format 的参考。

---

## Assimp

重点参考：

```text
Mesh
Bone
Animation
Node hierarchy
```

但：

> Assimp 是 Import Pipeline，不是 minEngine Animation Runtime Architecture。

---

# 38. 架构总图

最终希望逐步形成：

```text
                  Source Assets
               FBX / GLTF / ...
                       │
                       ▼
                   Assimp
                       │
                       ▼
                Asset Importer
                       │
             ┌─────────┼─────────┐
             ▼         ▼         ▼
           Mesh     Skeleton    Animation
             │         │         │
             └─────────┼─────────┘
                       ▼
                 Runtime Assets
                       │
                       ▼
              ┌─────────────────┐
              │ Animation System │
              │                 │
              │ Animator        │
              │ State Machine   │
              │ Player          │
              │ Clip            │
              │ Pose            │
              │ Blend           │
              │ Events          │
              └────────┬────────┘
                       │
                       ▼
                     Pose
                       │
                       ▼
                Bone Matrices
                       │
                       ▼
             SkeletalMeshComponent
                       │
                       ▼
                    Renderer
                       │
                       ▼
                      RHI
                 ┌─────┴─────┐
              OpenGL       Vulkan


Gameplay
   │
   ▼
Animator Parameters
   │
   ▼
State Machine
   │
   ▼
Animation
   │
   ▼
Animation Event
   │
   ├──────────→ Physics
   │
   ├──────────→ Audio
   │
   └──────────→ Gameplay
```

---

# 39. 最终设计哲学

开发 Animation System 时始终遵循：

> **Build the smallest system that creates a complete game-development loop.**

而不是：

> Build every feature a modern AAA animation system has.

优先级应该始终是：

```text
Correctness
    ↓
Clear Data Model
    ↓
Clean Runtime Boundary
    ↓
Game Usability
    ↓
Debuggability
    ↓
Performance
    ↓
Advanced Features
```

尤其是个人引擎：

> **一个清晰、可解释、可扩展、真正跑游戏的 Animation Runtime，远比一个拥有十几个高级功能但架构混乱的 Animation System 更有价值。**

---

# 40. Agent 工作方式

在实际开发中：

1. 先检查 minEngine 当前代码结构
2. 找到现有 Asset / Mesh / Renderer / Component / Serialization / Reflection 架构
3. 不要假设已有系统的接口
4. 优先复用现有 Engine Service / Interface
5. 不要为了 Animation 引入与现有架构冲突的新体系
6. 每完成一个阶段，提供：
   - 新增能力
   - 数据流变化
   - 架构变化
   - 下一阶段依赖
   - 测试方式
7. 每次只实现当前阶段真正需要的内容
8. 如果发现某个需求会导致架构复杂度显著上升，先停下来讨论，而不是直接实现
9. 不要为了“未来可能需要”提前实现 Tier 2 / Tier 3 功能
10. 优先建立可运行的垂直切片，而不是长时间建设没有可见结果的基础设施

---

# 41. 第一阶段的明确目标

现在正式开始 Animation System 时，第一阶段只关注：

```text
Skeleton
+
Skinned Mesh
+
GPU Skinning
+
Pose
```

目标 Demo：

```text
一个角色
    ↓
Skeleton
    ↓
Skinned Mesh
    ↓
手工生成一个 Pose
    ↓
GPU Skinning
    ↓
角色成功发生骨骼变形
```

确认这一层数学、数据结构、Renderer/RHI 边界完全正确后，再进入：

```text
Animation Clip
```

不要一开始同时开发：

```text
Skeleton
Animation
Animator
State Machine
Blend Tree
IK
```

---

# 42. Ultimate Goal

最终 minEngine Animation System 应该达到：

> **A lightweight, data-driven skeletal animation runtime that evaluates animation clips into poses, supports parameter-driven state transitions and blending, generates GPU skinning palettes, and exposes animation events to gameplay systems while remaining independent from the graphics backend and specific source asset formats.**

它的最终目的不是证明：

> “minEngine 实现了多少 Animation Feature。”

而是证明：

> **“minEngine 已经能够承载一个真正的 3D Game Runtime。”**