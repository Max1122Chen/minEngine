# RND-F25 -- Render-to-Texture 与 Clear/RenderTarget 配置 API

## Meta
- **ID:** RND-F25
- **类型:** Feature
- **状态:** Implemented (awaiting review)
- **作者:** agent + @mikumikumikumiku
- **最后更新:** 2026-10-28 (implementation S01-S05 complete)
- **关联项:**
  - [RND-F17](./RND-F17_ISOLATED_PREVIEW_DRAW_SCHEDULING_DESIGN.md) -- 多 View 隔离（已落地）
  - [TD-027](../TECH_DEBT.md) -- Scene3D 缩略图（已落地）
  - [ENGINE_0_1_0_ROADMAP](../ENGINE_0_1_0_ROADMAP.md) §3.4 IsoRT
  - `SceneDrawDesc` / `SceneViewport` / `SceneRenderTarget` / `PreviewScene` / `AssetThumbnailService`

## 参考引擎

| 引擎 | 关键概念 | 代码锚点 |
|------|----------|----------|
| **UE5** | `UTextureRenderTarget2D` 上挂 `ClearColor`（FLinearColor）；`FClearValueBinding`（EColorBound/EDepthStencilBound）在纹理创建时绑定到 RT 资源上；`FRHIRenderPassInfo` 承载 per-attachment 的 load/store + clear value。 | `Engine/Source/Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h:106-117`; `Engine/Source/Runtime/RHI/Public/RHIResources.h:248-327` |
| **Godot** | `Viewport` 有 `transparent_bg` 布尔属性、`own_world_3d`（可拥有独立的 World3D 做离屏渲染）、`clear_mode`（Always/Never/NextFrame）。`ViewportTexture` 通过 NodePath 引用一个 Viewport，将其渲染结果作为 Texture2D 读取。 | `scene/main/viewport.h:264,828,886` |

## TL;DR

将已有的 PreviewScene + SceneViewport + SceneRenderTarget 离屏渲染路径**产品化**为引擎运行时可调用的 Render-to-Texture 能力。以 `SceneDrawDesc` 作为唯一的渲染配置载体（不引入新的 Descriptor 类型），以 `PreviewScene` 作为离屏场景的唯一构建方式（游戏侧不直接 NewScene）。同时将硬编码的 clear color 参数化。

minCraft 在运行时：
1. 创建一个 PreviewScene，放入 BlockItem 的 mesh
2. 配置一个 Camera（斜45度视角）
3. 通过 OffscreenRenderer 提交到 ForwardRenderer 渲染一帧
4. 得到带透明背景的 RGBA8 贴图作为 item icon

---

## 设计

### 1) 核心抽象：`OffscreenRenderer`（thin wrapper）

`OffscreenRenderer` 不是新的渲染管线，只是对现有 `SceneViewport` + `SceneDrawDesc` + `ForwardRenderer` 提交路径的轻量封装。参考 UE `UTextureRenderTarget2D`（可配置的 RT）+ Godot `Viewport`（独立小世界 + transparent_bg）。

```
OffscreenRenderer
  Initialize(uint32_t width, uint32_t height)
  SetPreviewScene(PreviewScene*)       // 设置要渲染的预览场景
  SetCamera(RenderCamera*)             // 设置相机
  SetClearColor(LinearColor)           // 运行时修改 clear color（会更新内部 SceneDrawDesc）
  SetFlags(SceneDrawFlags)             // 运行时修改渲染标志
  RenderOnce() -> RHITextureRef        // 同步渲染一帧，返回颜色纹理
  GetColorTexture() -> const RHITextureRef&
  GetDepthTexture() -> const RHITextureRef&
  Shutdown()                           // 释放 GPU 资源
```

内部结构：
- 持有一个 `SceneViewport`（含 `SceneRenderTarget` + 内部分配的 `RenderCamera` + `ViewId`）
- 持有 `SceneDrawDesc`，初始化时填充，运行时通过 setter 更新
- `RenderOnce()` 内部：
  1. `m_SceneDrawDesc.Scene = m_PreviewScene->GetRenderScene()`
  2. （如果用内部 Camera）同步 aspect ratio
  3. `RenderSystem::SubmitSceneDraw(m_SceneDrawDesc)`
  4. 返回 `m_Viewport.GetColorTexture()`

**不引入新的 Descriptor 类型：** `SceneDrawDesc` 已经包含了 Scene、Camera、RenderTarget、ViewId、Flags，再加上 ClearColor/DepthClear 字段后就是完整的离屏渲染配置。不需要 `OffscreenRenderDesc`。

**输入是 PreviewScene 而非裸 Scene：**
- `Scene` 在引擎中既是资产又是运行时实例，NewObject<Scene> 不应该由游戏侧直接调用。
- `PreviewScene` 封装了"构建微型渲染场景"的职责（创建 Scene + GO + 资源管理 + GC 注册），正是离屏渲染的标准入口。
- 与 AssetThumbnailService 的缩略图路径天然统一：两边都用 PreviewScene 构建场景，都用 OffscreenRenderer 提交渲染。

### 2) SceneDrawDesc 扩展：ClearColor / DepthClear

`SceneDrawDesc` 新增两个字段：

```cpp
struct SceneDrawDesc {
    RenderScene* Scene = nullptr;
    Scene* GameplayScene = nullptr;
    RenderCamera* Camera = nullptr;
    SceneRenderTarget* RenderTarget = nullptr;
    uint64_t ViewId = 0;
    SceneDrawFlags Flags = SceneDrawFlags::Default;
    // 新增：
    LinearColor ClearColor = LinearColor(0.1f, 0.1f, 0.1f, 1.0f);
    float DepthClear = 1.0f;
};
```

默认值 `(0.1, 0.1, 0.1, 1.0)` 保持现有行为不变（主视口灰底、缩略图灰底）。

### 3) Clear Color 的去硬编码

**现状两处硬编码：**
- `SceneRenderTarget::BuildRenderPassInfo()` 第 84-87 行：`ClearValue.Color = {0.1, 0.1, 0.1, 1.0}`
- `RenderSystem.cpp` 第 60/72 行：`RHISetBackbufferClearColor(Vector3(0.1,0.1,0.1))`

**方案（对齐 UE FClearValueBinding）：**

1. `SceneRenderTarget::BuildRenderPassInfo()` 参数化：
```cpp
// Before:
RHIRenderPassInfo BuildRenderPassInfo() const;
// After:
RHIRenderPassInfo BuildRenderPassInfo(const LinearColor& clearColor, float depthClear) const;
```

2. `ForwardRenderer::EnqueueFrameRenderGraph()` 从 `SceneDrawDesc` 读取 clear value，传入 `BuildRenderPassInfo()`，再流入 RDG 各 pass 的 `RHIRenderPassInfo`。

3. `RHISetBackbufferClearColor` 保持不变 —— 只影响 swapchain 主视口（`PresentToBackBuffer` 路径），与离屏路径无冲突。

**为什么 clear value 放在 SceneDrawDesc 而非 SceneRenderTarget 成员上：**
- 同一个 RT 可被多个 View 以不同 clear color 渲染。
- Clear color 是 per-draw 的概念，不是 per-RT 的概念。

### 4) minCraft 调用方式

```cpp
// 游戏侧代码（minCraft）
void GenerateBlockItemIcon(BlockItemDef* def) {
    // 1. 用 PreviewScene 构建微型渲染场景
    PreviewScene preview;
    preview.BuildDefaultSphereScene();  // 搭建骨架 Scene
    preview.ResetPreviewMeshToDefaultSphere();
    preview.SetPreviewMesh(def->Mesh);
    preview.SetPreviewMaterial(def->Material);

    // 2. 配置离屏渲染器
    OffscreenRenderer renderer;
    renderer.Initialize(128, 128);
    renderer.SetClearColor(LinearColor(0, 0, 0, 0));  // 透明底色
    renderer.SetFlags(SceneDrawFlags::None);            // 无阴影/后处理/天空盒
    renderer.SetPreviewScene(&preview);

    // 3. 设置斜45度相机
    auto camera = std::make_shared<RenderCamera>();
    camera->m_Position = Vector3(1.2f, 1.0f, 1.2f);
    camera->m_ViewMatrix = glm::lookAt(
        camera->m_Position, Vector3(0,0,0), Vector3(0,1,0));
    camera->m_FOV = 30.0f;
    camera->m_AspectRatio = 1.0f;
    camera->UpdateProjectionMatrix();
    camera->UpdateViewProjMatrix();
    renderer.SetCamera(camera.get());

    // 4. 渲染并拿到纹理
    RHITextureRef result = renderer.RenderOnce();
    def->IconTexture = result;
}
```

### 5) 实现步骤（Slice 拆分）

| Slice | 内容 | 涉及文件 |
|-------|------|----------|
| **S01** | `SceneDrawDesc` 加 `ClearColor`/`DepthClear` 字段；`SceneRenderTarget::BuildRenderPassInfo()` 参数化；`ForwardRenderer` 透传至 RDG passes。 | `SceneDrawDesc.h`, `SceneRenderTarget.h/.cpp`, `ForwardRenderer.cpp`, `RenderPasses/` |
| **S02** | 新增 `OffscreenRenderer` 类：thin wrapper，接收 `PreviewScene*`，内部持有 `SceneViewport` + `RenderCamera` + `SceneDrawDesc`，`RenderOnce()` 提交到 `SubmitSceneDraw`。 | `Runtime/Function/Render/OffscreenRenderer.h/.cpp` |
| **S03** | 验证 `PresentToBackBuffer` 未置位时的离屏路径正确性；确保与主视口并存不串色（RND-F17 多 View 隔离已验证）。 | `ForwardRenderer.cpp`, `RenderSystem.cpp` |
| **S04** | 测试：单物体离屏渲染到透明背景；readback 验证 alpha 通道；`minEngineTests` 集成。 | `Tests/` |

S01 和 S02 互不依赖，可并行实施；S03 依赖 S01；S04 依赖所有前置 Slice。

### 6) 与 AssetThumbnailService 的统一

**当前缩略图路径：**
1. `m_InspectorViewport.Initialize(rhi, 256, 256)`
2. `m_InspectorPreviewScene.BuildDefaultSphereScene()`
3. `m_InspectorViewport.SetObservedScene(...)`
4. `m_InspectorViewport.BuildDrawDesc(EnablePostProcess)`
5. `RenderSystem::SubmitSceneDraw(desc)`
6. `GetColorTexture()` → `GetRHINativeTextureHandle()` → `ImTextureID`

**统一后（后续 Slice，本 Feature 不强制）：**
- `AssetThumbnailService` 内部改用 `OffscreenRenderer` 替代手工管理的 `SceneViewport`。
- `PreviewScene` 两边共享：缩略图的 `m_InspectorPreviewScene` 和 RenderToTexture 的 `PreviewScene` 是同一种用法。

本 Feature 中通过 S04 完成统一。完成后：AssetThumbnailService 不再持有独立的 SceneViewport + SceneRenderTarget，而是通过 OffscreenRenderer 提交缩略图渲染。PreviewScene 两边共享。

---

## 验证

- **S01:** 主视口 clear color 不变（默认灰底）；`SceneDrawDesc` 显式设 `ClearColor=(0,0,0,0)` 时渲染到透明。
- **S02:** `OffscreenRenderer` 创建/渲染/销毁通顺；纹理可作为 ShaderResource 采样。
- **S03:** 离屏渲染不触发 swapchain present；与主视口同帧不串色。
- **S04:** `minEngineTests`: 创建 `PreviewScene` → `OffscreenRenderer::RenderOnce()` → readback 验证 RGBA。