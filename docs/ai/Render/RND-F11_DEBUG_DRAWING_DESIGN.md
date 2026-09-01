# DebugDrawing — Design Spec

## Meta
- **ID:** `RND-F11`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-01
- **Branch:** `feat/debug-drawing`
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) · [PHYS-F03 placeholder](../Physics/PHYS-F03_CONTACT_GAMEPLAY_DISPATCH_DESIGN.md) · [ED-F01](../Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) · [RND-F03-M4 Pipeline Refactor](./RND-F03-M4_PIPELINE_REFACTOR_DESIGN.md)
- **Implementation:** [RND-F11_DEBUG_DRAWING_IMPLEMENTATION.md](./RND-F11_DEBUG_DRAWING_IMPLEMENTATION.md)

## TL;DR

为 minEngine 建立 **引擎级 Debug Visualization Channel**：各子系统通过 `DebugDraw` API 提交「画什么」，由独立 `DebugDrawPass`（Forward RDG 管线、Translucent 之后 / Post 之前）经 RHI 消费，**不**直接依赖 Vulkan/OpenGL。

**MVP（S01–S03）：** World-space 线/点/盒 wireframe；DepthTested；单帧 Transient；Editor 主视口 GL+VK 一致；优先服务 Physics collider / contact / trace 可视化。

**明确不做：** ImGui 3D 投影替代方案；`ManualRenderer` 挂 DebugPass；Standalone HUD；文本标签；**ForwardRenderer / `SceneDrawDesc` 感知 Physics 或 gameplay `Scene`**。

## Scope
- **In (MVP):**
  - `Runtime/Function/Debug/`：`DebugDrawService` + 几何展开 + 公开 API
  - `DebugDrawPass`：动态 VB、无光照 shader、读 `kRDGSceneColor` / `kRDGSceneDepth`
  - `SceneDrawFlags::EnableDebugDraw`；**仅**关卡编辑视口（`SceneEditingViewportClient`）默认开启 pass
  - `PhysicsDebugDraw`（`Runtime/Function/Physics/`）：collider / contact / trace → `DebugDraw::` 适配
  - `PhysicsContactEvent` 扩展 world `Position` / `Normal`（Jolt manifold）
  - ForwardRenderer RDG 插入 `Scene.Debug` pass
- **Out (initial):**
  - `ManualRenderer` 路径（诊断 renderer 保持最小差分，不挂 DebugPass）
  - UE `DrawDebugHelpers` 全量对等
  - Screen-space 文本、ImGui 混合排版
  - Persistent lifetime、Debug Categories / CLI toggle（Phase 3）
  - 线宽 &gt; 1px（VK 限制；MVP 接受 1px）
  - Lock-free 多线程命令收集（Phase 4）

## Reader quick start
1. 本文件：架构、数据结构、API、Pass 集成、Physics 消费
2. 下一文档：`RND-F11_DEBUG_DRAWING_IMPLEMENTATION.md`（切片 + DoD）
3. 代码入口（待建）：`Runtime/Function/Debug/`、`RenderPasses/DebugDrawPass.*`、`Shaders/DebugDraw.*`

---

## 1) 背景与目标

### 1.1 问题

Physics F01/F02 已提供 Contact、LineTrace、形状查询，但 **Editor 主视口无世界空间可视化**，调试依赖日志与单元测试断言。缺少统一出口也阻碍 Camera frustum、Shadow cascade、AI path 等后续调试需求。

### 1.2 目标

| 目标 | 说明 |
|------|------|
| **统一通道** | Physics / Editor / 未来子系统共用 `DebugDraw::` API，不各自碰 RHI |
| **深度一致** | Debug 几何写入场景 RT，可读 scene depth，collider 可被场景遮挡 |
| **GL/VK 一致** | 单条 RHI 路径；与 ED-F01 视口 parity 对齐 |
| **Physics 优先** | 首版验收：Box/Sphere/Capsule wireframe、contact 点+法线、LineTrace 线段+命中 |

### 1.3 非目标

- 不把 Debug Drawing 做成 Renderer 内部临时工具函数
- 不修改 `ShadowPass` / shadow 质量轨（并行在 `feat/render`）
- 不让 `ManualRenderer` 承担 DebugPass（保持 RND-F13 对照实验面最小）

---

## 2) 现状

| 组件 | 状态 |
|------|------|
| `DebugDraw` / `DrawDebug` 模块 | **不存在** |
| Forward RDG 顺序 | Shadow* → Sky → Opaque → Translucent → Post* → Present* |
| Scene 附件 | `kRDGSceneColor`、`kRDGSceneDepth`（Base/Translucent 写入） |
| `RHIBuffer::UpdateSubresource` | 可用于每帧 VB 上传 |
| `RHIPrimitiveType::LineList` | GL + VK 已支持 |
| Editor Gizmo | ImGuizmo overlay（屏幕空间，画在 RT **之上**）— 与 Debug Draw **分层** |
| Physics shapes | `BoxColliderComponent`、`SphereColliderComponent`、`CapsuleColliderComponent` |
| `PhysicsContactEvent` | 仅有 BodyId + Phase，**无** world position/normal |

---

## 3) 方案总览

### 3.1 架构（选定：方案 A）

曾比较 **B（ImGui ImDrawList 3D 投影）**：无法实现与 scene depth 一致的遮挡，VK 弱；仅适合 HUD。正式采用 **独立 DebugPass + 动态 VB**。

```text
┌──────────────────────────────────────────────────────────────┐
│  Call sites (Editor viewport, tests, future gameplay tools)   │
│    PhysicsDebugDraw::Submit* / DebugDraw::Line / …           │
│    — 在 SubmitSceneDraw **之前** 入队，Renderer 不感知来源      │
└────────────────────────────┬─────────────────────────────────┘
                             │ enqueue DebugPrimitive (CPU, main thread MVP)
┌────────────────────────────▼─────────────────────────────────┐
│  DebugDrawService          Runtime/Function/Debug/           │
│    - per-frame command queues（无 RHI、无 Physics）             │
│    - CPU tessellation → DebugVertex[]                        │
└────────────────────────────┬─────────────────────────────────┘
                             │ DebugDrawPass::Prepare()
┌────────────────────────────▼─────────────────────────────────┐
│  DebugDrawPass             RenderPipeline/RenderPasses/      │
│    - 动态 VB、PSO、上传、绘制；仅读 DebugDrawService 输出     │
└────────────────────────────┬─────────────────────────────────┘
                             │
                        RHI (GL / VK)
```

### 3.2 管线插入点

```text
Shadow*
  ↓
Scene.Sky
  ↓
Scene.Opaque
  ↓
Scene.Translucent
  ↓
Scene.Debug          ← 新增（仅 EnableDebugDraw + ForwardRenderer）
  ↓
Post.FXAA / Sharpen*
  ↓
Present*
```

`DebugDrawPass::SetupDependencies`：
- **Color** `kRDGSceneColor`：Load（不清除）
- **Depth** `kRDGSceneDepth`：DepthTest ON，DepthWrite OFF，Load existing depth

Post 仍读含 debug overlay 的 `SceneColor`，行为符合「调试几何属于场景的一部分」。

### 3.3 与 ImGuizmo / Overlay 分层

| 层 | 内容 | 时机 |
|----|------|------|
| Scene RT | 场景 mesh + **DebugDraw**（已入队原语） | `SubmitSceneDraw` → `ForwardRenderer::Execute` → `DebugDrawPass` |
| ImGui overlay | FPS、Gizmo（ImGuizmo） | `SceneEditingViewportWindow::OnDrawViewportOverlay` |

Debug 进 RT；Gizmo 不进 RT。两者不混用 API。

### 3.4 ManualRenderer

**不挂 DebugPass。** RND-F13 为 shadow/RDG 对照实验，刻意保持 pass 集合最小。需要 collider 可视化时使用默认 `ForwardRenderer` 或 Editor 视口。

---

## 4) 模块布局与依赖

```text
Runtime/Function/Debug/          ← 通用可视化框架（无 Physics、无 RHI）
  DebugDrawTypes.h
  DebugDrawService.h/.cpp
  DebugGeometry.h/.cpp
  DebugDraw.h/.cpp

Runtime/Function/Physics/        ← Physics 消费方适配（依赖 Debug，非反向）
  PhysicsDebugDraw.h/.cpp

Runtime/Function/Render/RenderPipeline/RenderPasses/
  DebugDrawPass.h/.cpp

Shaders/
  DebugDraw.vert / DebugDraw.frag

Runtime/Function/Physics/
  PhysicsTypes.h                 // PhysicsContactEvent +Position/Normal (S03)
```

**依赖规则（单向）：**

| 模块 | 可依赖 | 不可依赖 |
|------|--------|----------|
| `DebugDrawService` / `DebugDraw::` | Core, Math | RHI, RenderPipeline, Physics, Scene |
| `PhysicsDebugDraw` | `DebugDraw::`, Physics, Framework/Scene | RHI, RenderPipeline |
| `DebugDrawPass` | `DebugDrawService`, RHI, RenderGraph | Physics, Scene, `PhysicsDebugDraw` |
| `ForwardRenderer` | `DebugDrawPass`（pass 成员 + RDG 注册） | Physics, `PhysicsDebugDraw`, gameplay `Scene` |
| Editor viewport / tests | `DebugDraw::`, `PhysicsDebugDraw`, `RenderSystem` | — |

Physics 核心（`PhysicsWorld`）**不**内嵌绘制。`PhysicsDebugDraw` 由 **调用方**（Editor、测试）在 `SubmitSceneDraw` 之前显式调用，**不**由 `ForwardRenderer` 触发。

---

## 5) 数据结构

### 5.1 顶点与批次

```cpp
// DebugDrawTypes.h
struct DebugVertex
{
    Vector3 Position;
    Vector4 Color;   // RGBA, linear or sRGB 与现有 forward 一致（MVP: 直接传 display RGB）
};

/** 引擎中性 topology；`DebugDrawPass` 映射为 `RHIPrimitiveType`。 */
enum class EDebugPrimitiveTopology : uint8_t
{
    LineList = 0,
    // TriangleList — Phase 2+
};
```

MVP 每帧生成 **两个逻辑顶点流**（同结构，分 depth 模式绘制，对应两套 PSO）：
- `m_VerticesDepthTested`
- `m_VerticesAlwaysVisible`（Phase 2 启用；S01 可预留空队列）

### 5.2 深度与生命周期

```cpp
enum class EDebugDepthMode : uint8_t
{
    Tested = 0,        // 默认：LESS，写深度 OFF
    AlwaysVisible,     // Phase 2：深度测试 OFF
};

enum class EDebugLifetime : uint8_t
{
    Transient = 0,     // 当前帧（MVP 唯一模式）
    // Persistent — Phase 2
};
```

### 5.3 命令记录（CPU 队列，展开前）

调用方提交 **高级命令**；`DebugDrawService::BuildFrameGeometry()` 在 render Prepare 前统一展开为 `DebugVertex`。

```cpp
struct DebugLineCommand
{
    Vector3 Start;
    Vector3 End;
    Vector4 Color;
    EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
    EDebugLifetime Lifetime = EDebugLifetime::Transient;
};

struct DebugPointCommand
{
    Vector3 Position;
    float Size = 0.05f;   // 世界单位；MVP 用十字线近似，非 GL point sprite
    Vector4 Color;
    EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
};

struct DebugBoxCommand
{
    Matrix4 WorldTransform;   // 含位置/旋转/缩放
    Vector3 HalfExtent;       // 局部半轴
    Vector4 Color;
    EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
};

struct DebugSphereCommand
{
    Matrix4 WorldTransform;
    float Radius;
    Vector4 Color;
    uint32_t Segments = 16;   // 纬线/经线分段（S02）
    EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
};

struct DebugCapsuleCommand
{
    Matrix4 WorldTransform;
    float Radius;
    float HalfHeight;         // 沿局部 +Y 柱段半高（与 CapsuleCollider 一致）
    Vector4 Color;
    uint32_t Segments = 12;
    EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
};
```

`DebugTriangleCommand`、Arrow、Frustum、Capsule 以外的原语 — **Phase 2+**，接口预留命名空间即可。

### 5.4 服务内部状态

```cpp
class DebugDrawService
{
    // 入队（线程安全 Phase 4；MVP 仅 game/main thread）
    std::vector<DebugLineCommand>   m_Lines;
    std::vector<DebugPointCommand>  m_Points;
    std::vector<DebugBoxCommand>    m_Boxes;
  // S02: m_Spheres, m_Capsules

    // 展开结果（每帧重建；仅 CPU 数据，无 GPU 资源）
    std::vector<DebugVertex> m_VerticesDepthTested;
    std::vector<DebugVertex> m_VerticesAlwaysVisible;
};
```

> **注意：** 动态 `RHIBuffer` 由 **`DebugDrawPass` 独占**，不在 `DebugDrawService` 内持有，避免 Debug 核心层依赖 RHI。

---

## 6) 公开 API

### 6.1 命名与入口

顶层命名空间函数（UE `DrawDebugLine` 风格），内部转 `DebugDrawService::Get()`：

```cpp
namespace minEngine::DebugDraw
{
    void Line(const Vector3& start, const Vector3& end, const Vector4& color,
              EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    void Point(const Vector3& position, float size, const Vector4& color,
               EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    void Box(const Matrix4& worldTransform, const Vector3& halfExtent, const Vector4& color,
             EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    // S02
    void Sphere(const Matrix4& worldTransform, float radius, const Vector4& color, ...);
    void Capsule(const Matrix4& worldTransform, float radius, float halfHeight, const Vector4& color, ...);
}
```

**帧边界（由 `DebugDrawPass::Prepare` 驱动，调用方无需参与）：**

```cpp
// DebugDrawService — 由 DebugDrawPass::Prepare 调用，非 gameplay API
void BuildFrameGeometry();    // 读取当前队列 → m_Vertices*
void ClearFrameQueues();      // Build 之后：释放已展开的 transient 命令
```

**约定：**
- 坐标均为 **世界空间**
- Color：`Vector4(r, g, b, a)`，alpha=0 视为不透明 1.0
- 默认 `EDebugDepthMode::Tested`
- MVP 仅 `Transient`；无 Persistent API

### 6.2 DebugDrawService 方法

```cpp
class DebugDrawService
{
public:
    static DebugDrawService& Get();

    void ClearFrameQueues();
    void BuildFrameGeometry();   // tessellate → m_Vertices*；调用方随后 Clear

    const std::vector<DebugVertex>& GetVertices(EDebugDepthMode mode) const;
    uint32_t GetVertexCount(EDebugDepthMode mode) const;
    bool HasAnyGeometry() const;

    // 低级入队（供 DebugDraw:: 包装）
    void EnqueueLine(DebugLineCommand cmd);
    void EnqueuePoint(DebugPointCommand cmd);
    void EnqueueBox(DebugBoxCommand cmd);
};
```

### 6.3 Physics 适配层

位于 **`Runtime/Function/Physics/`**（Physics 域依赖 Debug API，而非 Debug 模块依赖 Physics）。

```cpp
namespace minEngine::PhysicsDebugDraw
{
    struct Options
    {
        bool bDrawColliders = true;
        bool bDrawContacts = true;
        bool bDrawActiveTrace = false;   // 最近一次 trace（Editor 设置）
        float ContactNormalLength = 0.15f;
    };

    /** 从 Scene + PhysicsWorld 提交本帧 debug 原语 */
    void SubmitScene(const Scene& scene, const PhysicsWorld& world, const Options& options);

    /** LineTrace 结果可视化（测试 / Editor 工具调用） */
    void SubmitLineTrace(const Vector3& start, const Vector3& end, const HitResult& hit);
}
```

**Collider 颜色（默认，可配置）：**

| `ECollisionChannel` | 颜色（RGB） |
|---------------------|-------------|
| `WorldStatic` | 灰绿 `(0.4, 0.8, 0.4)` |
| `Default` | 青 `(0.2, 0.9, 1.0)` |
| `Trigger` | 黄 `(1.0, 0.9, 0.2)` |
| 其他 | 白 `(1,1,1)` |

---

## 7) 几何展开（CPU）

GPU 不识别「盒/球」高级原语；**第一版全部 CPU 展开**。

| 原语 | 展开策略 | Topology |
|------|----------|----------|
| **Line** | 2 vertices | `LineList` |
| **Point** | 3 条短线（Y/Z/X 轴十字）或 2D 屏幕 facing 十字（MVP 用固定世界轴十字，size=半长） | `LineList` |
| **Box** | 12 条边 × 2 vertices = 24 vertices（局部 ±HalfExtent 变换到世界） | `LineList` |
| **Sphere** | 3 个正交圆（经/纬/赤道），每圆 `Segments` 线段 | `LineList` |
| **Capsule** | 上下半球弧 + 4 条竖向母线 + 顶部/底部圆 | `LineList` |

实现放在 `DebugGeometry.cpp` 为 **静态成员函数**（如 `DebugGeometry::AppendBoxWireframe(...)`），由 `DebugDrawService::BuildFrameGeometry` 调用。

**线宽：** MVP 使用 RHI 默认 **1px** `LineList`；不引入 camera-facing quad 粗线（Phase 2）。

---

## 8) DebugDrawPass

### 8.1 类职责

```cpp
class DebugDrawPass : public RenderPassBase, public IRenderPass
{
public:
    void Setup(RHI& rhi) override;                    // PSO, shader, layout, VB 分配
    void SetupDependencies(RenderPass& self, RenderGraph& graph) override;
    void Prepare(RenderGraph& graph) override;        // BuildFrameGeometry + buffer upload
    void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override;

private:
    RHIBufferRef m_VertexBuffer;
    RHIVertexInputLayoutRef m_VertexLayout;
    RHIGraphicsPipelineStateRef m_PsoDepthTested;
    RHIGraphicsPipelineStateRef m_PsoAlwaysVisible;   // Phase 2
    RHIShaderRef m_VertexShader;
    RHIShaderRef m_PixelShader;
};
```

### 8.2 RDG 依赖

```cpp
void DebugDrawPass::SetupDependencies(RenderPass& self, RenderGraph& graph)
{
    (void)graph;
    self.AddTextureInput(kRDGSceneColor);   // read existing color
    self.AddColorOutput(kRDGSceneColor, MakeSceneColorAttachment());  // load
    self.SetDepthStencilOutput(kRDGSceneDepth, MakeSceneDepthAttachment()); // load + test
}
```

与 `TranslucencyPass` 声明模式一致；**不** `AddSceneLitShadowTextureInputs`（debug 无光照）。

### 8.3 Prepare / Draw 流程

```text
Prepare:
  1. DebugDrawService::BuildFrameGeometry()   // 消费本帧已入队命令
  2. if vertexCount == 0 → skip draw（仍可 Clear 队列）
  3. grow m_VertexBuffer if needed
  4. m_VertexBuffer->UpdateSubresource(...)
  5. DebugDrawService::ClearFrameQueues()   // 展开后清空，供下一帧入队
```

BuildRenderPass:
  1. BeginRenderPass(SceneColor + SceneDepth, load both)
  2. Bind PerFrame UBO (binding 0, 复用 ForwardRenderer 同一 buffer)
  3. Set PSO (DepthTested): depth test LESS, depth write false, cull none, LineList
  4. Set VB, Draw(vertexCount, 1)
  5. (Phase 2) second draw for AlwaysVisible PSO
  6. EndRenderPass
```

### 8.4 PerFrame UBO

复用 `ForwardRenderer::m_PerFrameUniformBuffer` 与 `PerFrameData`（View / Proj / ViewProj）。Debug shader 仅乘 `ViewProj`，无光照 uniform。

`DebugDrawPass` 通过 `RenderPassBase::pipeline`（`ForwardRenderer*`）访问该 buffer，与 `BasePass` 取 scene bindings 方式一致。

---

## 9) Shader

### 9.1 DebugDraw.vert

```glsl
#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(set = 0, binding = 0) uniform PerFrame
{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 CameraPos;
} u_PerFrame;

layout(location = 0) out vec4 v_Color;

void main()
{
    gl_Position = u_PerFrame.ViewProj * vec4(inPosition, 1.0);
    v_Color = inColor;
}
```

### 9.2 DebugDraw.frag

```glsl
#version 450
layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = v_Color;
}
```

### 9.3 PSO 固定状态

| 状态 | DepthTested (MVP) |
|------|-------------------|
| Primitive | `LineList` |
| Cull | None |
| Depth test | Less |
| Depth write | **false** |
| Blend | Off（opaque lines） |
| Color write | RGBA8 ON |

Vertex layout：`position` Float3 @0，`color` Float4 @1，stride = `sizeof(DebugVertex)`。

---

## 10) 动态 Vertex Buffer

```text
Debug queues
  → BuildFrameGeometry()
  → contiguous DebugVertex[]
  → RHIBuffer::UpdateSubresource (host-visible 或 staging，与现有 VB 路径一致)
  → DebugDrawPass draw
```

**S01 策略：**
- 初始容量：例如 16 KiB vertices（可调）
- 每帧 `vertexCount > capacity` 时 **重新分配** 更大 buffer（×2）
- 不做 ring buffer / frame allocator（Phase 4）

**上限（软）：** 单帧超过 ~1M vertices 时 `ME_CORE_WARN` 并截断，防止 Editor 卡死。

---

## 11) 帧生命周期

**原则：提交（enqueue）在 Renderer 外；消费（tessellate + draw）在 `DebugDrawPass` 内。**

```text
Frame N — 关卡编辑视口（SceneEditingViewportClient 为例）
│
├─ Physics Step / Sync（如有）
├─ if EnableDebugDraw:
│     ├─ PhysicsDebugDraw::SubmitScene(activeScene, physicsWorld, options)
│     └─ （或测试 / 工具直接 DebugDraw::Line …）
├─ SceneDrawDesc desc = viewport.BuildDrawDesc(flags | EnableDebugDraw)
├─ RenderSystem::SubmitSceneDraw(desc)     // desc.Scene 仍为 RenderScene*
│     └─ ForwardRenderer::Execute
│           ├─ UpdatePerFrameUBO
│           ├─ SetupFrameRenderGraph（含 Scene.Debug if flag）
│           ├─ RDG Enqueue
│           │     └─ DebugDrawPass::Prepare
│           │           ├─ BuildFrameGeometry（读本帧队列）
│           │           ├─ Upload VB
│           │           └─ ClearFrameQueues
│           │     └─ DebugDrawPass::BuildRenderPass → draw
│           └─ Present …
```

**单帧时序（关键）：**

```text
1. 调用方入队（PhysicsDebugDraw / DebugDraw::…）
2. SubmitSceneDraw(desc)
3. DebugDrawPass::Prepare：Build → Upload → Clear 队列
4. DebugDrawPass::BuildRenderPass：绘制上一步生成的顶点
```

**`SceneDrawFlags`：**

```cpp
enum class SceneDrawFlags : uint32_t
{
    // ...
    EnableDebugDraw = 1u << 4,
};
```

- **Pass 门控：** `ForwardRenderer` 仅根据 flag 注册/执行 `Scene.Debug`。
- **提交门控：** Editor / 测试在调用 `PhysicsDebugDraw` 前同样检查 flag（避免无效 CPU 展开）。
- **默认：** `SceneEditingViewportClient` 打开 flag；Thumbnail / Material Preview **不**开。

**禁止：** 在 `ForwardRenderer::Execute` 内调用 `PhysicsDebugDraw`；向 `SceneDrawDesc` 增加 gameplay `Scene*` 仅为 debug 服务。

---

## 12) Physics 集成

### 12.1 Collider wireframe（S02）

由 **`PhysicsDebugDraw::SubmitScene`**（Editor 在 `SubmitSceneDraw` 前调用）遍历 gameplay `Scene`：
1. 取 `SceneComponent` 世界 `Matrix4`
2. 按组件类型调用 `DebugDraw::Box` / `Sphere` / `Capsule`
3. 颜色由 `GetObjectChannel()` 查表

需访问 `Scene` 组件迭代 API（与现有 physics sync 遍历一致）。

### 12.2 Contact 可视化（S03）

**扩展 `PhysicsContactEvent`：**

```cpp
struct PhysicsContactEvent
{
    PhysicsBodyId BodyA{InvalidPhysicsBodyId};
    PhysicsBodyId BodyB{InvalidPhysicsBodyId};
    ECollisionResponse Response{ECollisionResponse::Block};
    EContactPhase Phase{EContactPhase::Begin};
    Vector3 Position{};   // 新增：manifold 上一点（世界空间）
    Vector3 Normal{};     // 新增：自 BodyB 指向 BodyA 或 Jolt 约定法线（文档写明）
};
```

在 `ContactListenerImpl::OnContactAdded` 中从 `JPH::ContactManifold` 读取（例如 `GetWorldSpaceContactPointOn1(0)` 与 `GetWorldSpaceNormal()`）。`OnContactRemoved` 的 End 事件可填 last known 或 position=0（仅画 Begin 接触）。

可视化：
- `DebugDraw::Point(position, 0.08f, contactColor)`
- `DebugDraw::Line(position, position + normal * ContactNormalLength, normalColor)`

仅 `Phase == Begin` 且 `Response != Ignore` 时绘制；End 事件可选淡出（Phase 2 Persistent）。

### 12.3 LineTrace（S03）

```cpp
void PhysicsDebugDraw::SubmitLineTrace(const Vector3& start, const Vector3& end, const HitResult& hit)
{
    const Vector4 missColor(1.f, 0.2f, 0.2f, 1.f);
    const Vector4 hitColor(0.2f, 1.f, 0.2f, 1.f);
    if (hit.bHit)
    {
        DebugDraw::Line(start, hit.Location, hitColor);
        DebugDraw::Line(hit.Location, end, missColor * 0.5f);
        DebugDraw::Point(hit.Location, 0.06f, hitColor);
        DebugDraw::Line(hit.Location, hit.Location + hit.Normal * 0.2f, Vector4(0.2f, 0.6f, 1.f, 1.f));
    }
    else
    {
        DebugDraw::Line(start, end, missColor);
    }
}
```

---

## 13) 线程与扩展（后续）

| Phase | 内容 |
|-------|------|
| **1 (S01)** | 主线程 only；`DebugDrawService` 无锁 |
| **2** | `AlwaysVisible` depth mode；Persistent lifetime（秒级 / 手动清除） |
| **3** | `debug.draw physics on/off` CLI；category 位掩码 |
| **4** | 渲染线程消费；lock-free 或 double-buffer 命令队列；VB ring buffer |

API 设计保持 **enqueue 与 render 解耦**，便于 Phase 4 把 `BuildFrameGeometry` 移到 render thread 前合并。

---

## 14) 实施切片（概要）

详细 DoD 见 Implementation Plan。

| Slice | 交付 | 验收 |
|-------|------|------|
| **RND-F11-S01** | `DebugDrawTypes/Service/Geometry`、`DebugDrawPass`、shader、RDG 插入、`EnableDebugDraw` | Editor 视口调用 `DebugDraw::Line` 可见；`verify.ps1`；GL+VK 目视 |
| **RND-F11-S02** | Sphere/Capsule wireframe、`PhysicsDebugDraw` collider 遍历 | 场景中带 Collider 的 GO 显示 wireframe |
| **RND-F11-S03** | `PhysicsContactEvent` 扩展、contact + `SubmitLineTrace` | contact 点/法线；trace 线段目视 |
| **RND-F11-S04** | Debug console / CLI category toggle | `debug.draw physics off` 关闭 collider |

---

## 15) 验收标准

### 15.1 MVP（S01–S03）

- [ ] Editor 主视口（`--rhi vulkan` / `opengl`）可见 **Box** collider wireframe
- [ ] `LineTrace` 命中：线段、命中点、法线可辨
- [ ] Contact **Begin**：接触点 + 法线（扩展 `PhysicsContactEvent` 后）
- [ ] Debug 几何 **被场景 mesh 遮挡**（DepthTested）
- [ ] `verify.ps1` 与相关 physics tests 仍通过
- [ ] **未修改** `ShadowPass`；`feat/render` shadow 轨可并行

### 15.2 工程

- [ ] `DebugDrawService` 不依赖 RHI / Physics / Scene
- [ ] `DebugDrawPass` 不依赖 Physics / `PhysicsDebugDraw`
- [ ] `ForwardRenderer` 不 `#include` Physics；不调用 `PhysicsDebugDraw`
- [ ] `SceneDrawDesc` 不新增 gameplay 字段
- [ ] `ManualRenderer` **无** DebugPass
- [ ] Implementation Plan + PROGRESS_LOG 条目

---

## 16) 风险与缓解

| 风险 | 缓解 |
|------|------|
| VK `LineList` 线宽恒为 1 | MVP 接受；Phase 2 用细三角条模拟 |
| 每帧大量 collider 顶点过多 | 软上限 + warn；远景 LOD 后续 |
| `PhysicsContactEvent` 扩展破坏测试 | 更新 `PhysicsContactTest` 断言；新字段对旧逻辑默认 `{}` |
| DebugPass 与 Translucent 深度冲突 | 同一 depth attachment Load；debug 不写深度 |
| 忘记清队列导致残影或重复 | `Prepare` 内 **Build 之后** `ClearFrameQueues` |

---

## 17) 已决事项

| # | 决策 | 日期 |
|---|------|------|
| 1 | 方案 A（DebugPass + RHI），非 ImGui 3D | 2026-08-31 |
| 2 | `DebugDrawService` 置于 `Runtime/Function/Debug/` | 2026-08-31 |
| 3 | Contact 可视化前扩展 `PhysicsContactEvent`（Position + Normal） | 2026-08-31 |
| 4 | MVP 线宽 1px | 2026-08-31 |
| 5 | `ManualRenderer` **不**挂 DebugPass | 2026-08-31 |
| 6 | **提交在 Renderer 外**（Editor/测试在 `SubmitSceneDraw` 前）；`ForwardRenderer` 不感知 Physics | 2026-09-01 |
| 7 | `PhysicsDebugDraw` 放在 `Physics/`，非 `Debug/` | 2026-09-01 |
| 8 | 动态 VB 仅 `DebugDrawPass` 持有；`DebugDrawTypes` 不用 RHI 类型 | 2026-09-01 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 占位 Draft；分支 `feat/debug-drawing` 开轨 |
| 2026-08-31 | 完整 Design：架构、数据结构、API、Pass、Physics、切片、验收 |
| 2026-09-01 | 解耦修订：提交在 Renderer 外；`PhysicsDebugDraw` → Physics/；无 `GameplayScene`；VB 归 Pass；队列 Clear 时序 |
