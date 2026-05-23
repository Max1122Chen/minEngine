# 渲染重构计划：Viewport + SceneRenderContext

> 前置：`RuntimeGlobalContext` 已收敛到 `Engine`（commit `c885f63`）。  
> 目标：多 Viewport / 多 Scene 绘制；`RenderSystem` 仅作渲染服务；Pass 逻辑基本不变。  
> UE 参照：`FScene`（内容）+ `FSceneViewFamily`（Scene + RenderTarget + 相机）+ 视口持 RT（`FEditorViewportClient` / `UGameViewportClient`）。

---

## 1. 目标架构（共识）

```text
┌─────────────────────────────────────────────────────────────┐
│ RenderSystem（服务，无「当前关卡」状态）                      │
│   RHI、RenderPipeline 执行器、ShadowResourceManager 池        │
│   SubmitSceneDraw(SceneDrawDesc) / Tick() 消费队列            │
│   CreateOrResizeSceneRenderTarget(...)  // 工厂，可选         │
└─────────────────────────────────────────────────────────────┘
         ▲ Submit                    │
         │                           ▼ Execute(desc) 内建 SceneRenderContext
┌────────┴────────┐         ┌────────────────────────┐
│ SceneViewport   │         │ RenderPipeline         │
│  SceneRenderTarget（拥有）│  Pass 实例 + 共享 UBO   │
│  RenderCamera（拥有）     │  本帧队列在 Context 里   │
│  Observed RenderScene*（引用）│                        │
└────────┬────────┘         └────────────────────────┘
         │ 引用
┌────────┴────────┐
│ Scene（逻辑资产）│
│  shared_ptr<RenderScene>  // 仅 Proxy 集合，无 RT
└─────────────────┘

Editor：`SceneEditingViewportWindow` + `SceneEditingViewportClient` → 拥有 `SceneViewport`（场景编辑主视口）
Play：  Engine / Play 模块 → **GameMainViewport（待定，见 §11）**
Preview：`MaterialPreviewViewportWindow` + Preview `RenderScene` + 独立 RT

> **Playground：** 长期未使用、不维护；见 `Playground/src/Playground.cpp` 顶部说明。**P4 暂缓。**
```

**不变：** Shadow → Base → Translucent → PostProcess 顺序；`MeshDrawCommand` 结构；材质/阴影算法。  
**变：** 全局 `RenderSystem::m_RenderScene` / `m_MainCamera` / 单一 `m_SceneColorTexture` → 按 Viewport 提交。

---

## 2. 新增类型（Runtime）

| 类型 | 文件建议 | 职责 |
|------|----------|------|
| `SceneRenderTarget` | `Render/SceneRenderTarget.h` | Color/Depth/FB + 宽高；**Viewport 拥有** `shared_ptr` |
| `SceneViewport` | `Render/SceneViewport.h/.cpp` | RT + Camera + Flags + `ObservedScene`；`BuildDrawDesc()`、`RequestResize()` |
| `SceneDrawDesc` | `Render/SceneDrawDesc.h` | 提交给 `RenderSystem` 的轻量 POD |
| `SceneDrawFlags` | 同上 | Shadow / PostProcess / PresentToBackBuffer |
| `SceneRenderContext` | `Render/SceneRenderContext.h` | **本帧**：Opaque/Translucent 队列、ShadowRequests、Handles |

---

## 3. 分阶段计划

### 阶段 P0 — 文档与骨架（本文件 + 空类型）

**改动面**

- 新增头文件：`SceneDrawDesc.h`、`SceneRenderTarget.h`、`SceneRenderContext.h`、`SceneViewport.h`（可先只有声明）
- CMake：加入新 `.cpp`（若实现为空）

**验收**

- [x] 工程编译通过
- [x] 无运行时行为变化

**Editor 可见性：** 与现在一致（无变化）。

---

### 阶段 P1 — Pipeline 参数化 + SceneRenderContext（仍单视口、可 Editor 看）

**目的：** 去掉 `RenderPipeline` / Pass 对 `RenderSystem::Get().m_RenderScene` 的硬编码；队列迁到 Context。

**改动面（核心）**

| 区域 | 文件 | 改动要点 |
|------|------|----------|
| Pipeline 入口 | `RenderPipeline.h/.cpp` | `Execute(const SceneDrawDesc&)`；`Execute()` 旧接口删除或内部转调 |
| 本帧状态 | `SceneRenderContext` | `BuildRenderQueue(ctx)`、`CollectShadow*(ctx)` 等 |
| Pipeline 成员 | `RenderPipeline.h` | **移除** `m_OpaqueQueue`、`m_TranslucentQueue`、`m_ShadowRequests`（迁到 Context） |
| 硬编码清理 | `RenderPipeline.cpp` | `desc.Scene` / `desc.Camera` 替代 `Get().m_RenderScene` / `GetMainCamera()` |
| Pass | `ShadowPass.cpp`、`TranslucencyPass.cpp` | 仍从 Pass 成员读队列（Execute 内从 ctx 拷贝，**最小改 Pass 签名**） |
| Pass | `BasePass.cpp`、`PresentPass.cpp`、`PostProcessPass.cpp` | 仅 RHI 仍 `RenderSystem::Get().GetRHI()`（可接受） |

**过渡（本阶段仍单 RT）**

- `RenderPipeline` **暂时保留** 创建主 `m_SceneBuffer` / `m_SceneColorTexture`（或迁到 `SceneRenderTarget` 但仅一份）
- `RenderSystem` **仍保留** `m_RenderScene`、`m_MainCamera`（P2 再删），`Tick()` 内构造一条 `SceneDrawDesc` 调用 `Execute(desc)`

**验收**

- [x] 编译通过（minEngine + Editor Debug）
- [ ] **Editor 主视口**：加载关卡、导航、阴影、材质、Gizmo、选中与 P1 前一致（需本地目视）
- [ ] `--material-ir-test` 仍通过（未在本轮自动跑）
- [x] 全局搜索无 `RenderSystem::Get().m_RenderScene`（`RenderPipeline` 内）

**Editor 可见性：** **必须可正常看**（本阶段目标是行为不变）。

**UE 对照：** `FSceneRenderer` 用 `FScene*` + View 矩阵，而非模块全局 Scene。

---

### 阶段 P2 — Scene 拥有 RenderScene + RenderSystem 服务化 + 主 SceneViewport

**目的：** `RenderScene` 从 `RenderSystem` 迁到 `Scene`；主视口资源进 `SceneViewport`；`SubmitSceneDraw` 正式化。

**改动面**

| 区域 | 文件 | 改动要点 |
|------|------|----------|
| 逻辑场景 | `Scene.h/.cpp` | `shared_ptr<RenderScene> m_RenderScene`；加载/Reset 时创建 |
| 场景管理 | `SceneManager.cpp` | 不再 `RenderSystem::m_RenderScene`；`GetRenderScene()` → 当前 Scene |
| 渲染服务 | `RenderSystem.h/.cpp` | 删除 `m_RenderScene`、`m_MainCamera`、`GetSceneColorTexture()`（或 deprecated 转发） |
| | | 新增 `SubmitSceneDraw`、`m_PendingDraws`、`Tick()` 循环 Execute |
| 主视口 | `SceneViewport.h/.cpp` | `EditorSceneViewport` 或 `SceneManager` 持有一个 `SceneViewport m_MainEditorViewport` |
| RT 迁移 | `RenderPipeline` | 主 FB/Color 创建迁到 `SceneRenderTarget::Initialize(RHI, w, h)` |
| 相机 | `RenderSystem` | 删除 `m_MainCamera`；主相机进 `SceneViewport::m_Camera` |
| 组件 | `CameraComponent.cpp` | `SetSelfAsMainCamera` 改为绑定 **GameMainViewport**（P2 可先仍写 Viewport 指针，Editor 飞行相机不依赖 Component） |
| 提交 | `Engine.cpp` 或 `SceneManager` | 每帧 `BuildDrawDesc(activeScene->GetRenderScene())` + `Submit` |

**仍可不碰 Editor UI**

- `ViewportWindow` 仍可通过 **兼容 API** 取主 Color：`SceneManager::GetMainViewport().GetColorTexture()` 或临时 `RenderSystem::GetPrimaryColorTexture()` 转发

**验收**

- [ ] Editor 主视口视觉与交互与 P1 一致（需本地目视）
- [x] 切换/加载 Scene 后 `EnsureRenderScene` + `SyncMainViewportObservedScene`
- [x] `RenderSystem` 无 `m_RenderScene` / `m_MainCamera` 成员
- [x] 每帧 `SubmitMainEditorViewportDraw` → `SubmitSceneDraw` → `Execute`

**Editor 可见性：** **必须可正常看**。

**若本阶段未完成 Editor 接线：** 可在 `RenderSystem::Tick` 内硬编码 Submit 一条 desc（使用 SceneManager 的主 Viewport），**仍算 P2 验收通过**；Editor 壳迁移算 P3。

**UE 对照：** `FScene` 与 `FRenderTarget` 分离；`FSceneViewFamily( RenderTarget, Scene, ShowFlags )`。

---

### 阶段 P2b — Editor 视口窗口分层（命名已拍板）

> 详细设计见 [EDITOR_VIEWPORT_WINDOWS.md](../Editor/EDITOR_VIEWPORT_WINDOWS.md)。

**拍板命名：** UI 基类 **`EditorViewportWindow`**；关卡编辑窗口 **`SceneEditingViewportWindow`**（不用 Level，避免与 Scene 混用）；Client **`SceneEditingViewportClient`**。

**目的：** 从现 `ViewportWindow` 拆出通用 RT 面板基类；Gizmo/场景 Overlay 仅留在场景编辑子类；为 Material 预览等窗口铺路。

**改动面**

| 区域 | 文件 | 改动要点 |
|------|------|----------|
| 新增 | `EditorViewportWindow.*` | 固定 `OnDraw`：Image + `ViewportFrameState` + `EndFrame` |
| 新增 | `EditorViewportClient` 基类 | 帧状态、RT 缩放钩子（实现从现 Client 上移） |
| 改名 | `ViewportWindow` → **`SceneEditingViewportWindow`** | Gizmo、Draggable Overlay 保留在子类 |
| 改名 | `EditorViewportClient` → **`SceneEditingViewportClient`** | 飞行/拾取/Gizmo 逻辑不变 |
| 注册 | `EditorGUIManager.cpp` | 默认注册 `SceneEditingViewportWindow` |

**验收**

- [x] 编译通过
- [ ] **SceneEditing** 主视口与 P2 行为一致（Gizmo、导航、拾取、缩放；需本地目视）
- [x] 仍使用 `SceneManager` 兼容 API（P3 再迁到 Client 的 `SceneViewport`）

**Editor 可见性：** **必须可正常看**。

**与 P3 关系：** P2b 可与 P3 并行；建议先 P2b 再 P3，避免 P3 接线时同时大改类名。

---

### 阶段 P3 — SceneEditingViewportClient 拥有 SceneViewport

**目的：** Editor 壳与 Runtime Viewport 对齐；resize / 相机 / Submit 从 Client 发出。

**改动面**

| 区域 | 文件 | 改动要点 |
|------|------|----------|
| Editor Client | **`SceneEditingViewportClient`**.h/.cpp | 成员 `SceneViewport m_Viewport`；`SyncRenderTargetSize` → viewport resize |
| | | 导航写 `m_Viewport.GetCamera()`；拾取用 viewport 的 Camera + ObservedScene |
| UI | **`SceneEditingViewportWindow`**.cpp | `GetDisplayColorTexture()` → client 的 `SceneViewport` |
| 提交 | `SceneEditingViewportClient::EndFrame` | `SetObservedScene` + `SubmitSceneDraw`；移除 `SceneManager::SubmitMainEditorViewportDraw` |
| 清理 | `SceneManager` | 移除主 `SceneViewport` 代持 |
| Present | `Editor.cpp` | `SceneDrawFlags` 不含 `PresentToBackBuffer` |

**验收**

- [x] **SceneEditing** 视口：缩放面板后 RT 尺寸跟随
- [x] WASD / 鼠标look、Gizmo、拾取正常
- [x] Editor 路径无 `SceneManager::GetMainCamera()` / `GetSceneColorTexture()` 硬编码

**Editor 可见性：** **必须可正常看**。

**UE 对照：** `FEditorViewportClient::CalcSceneView` + `FViewport` RenderTarget。

---

### 阶段 P4 — GameMainViewport（Playground / 非 Editor）— **暂缓**

**状态：** 不做。Playground 已废弃；GameRuntime 视口见 **§11 待定事项**。

**目的（原规划）：** 运行时非 Editor 路径有独立 Viewport；`CameraComponent` 驱动 Game 视口。

**改动面**

| 区域 | 文件 | 改动要点 |
|------|------|----------|
| Engine | `Engine.h/.cpp` | `SceneViewport m_GameViewport`（或 Play 子系统持） |
| Playground | `Playground.cpp` | 使用 `GameViewport` Submit，不用 Editor 视口 |
| Camera | `CameraComponent.cpp` | `SetSelfAsMainCamera` → 注册到 `GameMainViewport` |
| 窗口 | `WindowSystem` | Game 全屏时可选 `PresentToBackBuffer` |

**验收**

- [ ] Playground 启动：场景可见、相机由 Component 控制（若 Playground 仍使用）
- [ ] Editor 与 Playground 不抢同一 `RenderCamera` 实例

**Editor 可见性：** Editor 不受影响；Playground 单独验收。

**说明：** 若短期只做 Editor，**P4 可延后**。

---

### 阶段 P5 — MaterialPreviewViewport（第二 Scene + 第二 RT）

**目的：** 为 Material Editor 铺路；同一套 Submit，第二条 desc。

**改动面**

| 区域 | 文件 | 改动要点 |
|------|------|----------|
| Preview Runtime | `MaterialPreviewViewport` 或专用 `SceneViewport` 配置 | 独立 `RenderScene`（球+灯）、小 RT |
| Preview Editor | **`MaterialPreviewViewportWindow` / Client**（见 [EDITOR_VIEWPORT_WINDOWS.md](../Editor/EDITOR_VIEWPORT_WINDOWS.md)） | 无 Gizmo；`EndFrame` 第二条 Submit |
| Flags | `SceneDrawFlags` | Preview 关闭 Shadow / Present |

**验收**

- [ ] 主 Editor 视口仍正常（请本地目视）
- [ ] Preview Submit 不污染主 RT（两路 Color 不同纹理 ID）
- [x] 临时调试窗口 `Material Preview` 显示 Preview RT（**不要求**完整 Material Editor UI）

**Editor 可见性：** 主视口必须正常；Preview 可用 **临时 Debug 窗口** 验收，**不要求** Material Editor 成品 UI。

**UE 对照：** `FPreviewScene` + 独立预览视口 RT。

---

## 4. 文件级改动清单（汇总）

### 新增

```
minEngine/.../Render/SceneDrawDesc.h
minEngine/.../Render/SceneRenderTarget.h
minEngine/.../Render/SceneRenderTarget.cpp
minEngine/.../Render/SceneRenderContext.h
minEngine/.../Render/SceneViewport.h
minEngine/.../Render/SceneViewport.cpp
minEngine/.../Render/MaterialPreviewViewport.h   // P5
minEngine/.../Render/MaterialPreviewViewport.cpp // P5
```

### 大改

```
RenderPipeline.h / .cpp
RenderSystem.h / .cpp
Scene.h / .cpp
SceneManager.h / .cpp
```

### 中改（去全局 Get）

```
RenderPipeline/RenderPasses/ShadowPass.cpp
RenderPipeline/RenderPasses/TranslucencyPass.cpp
RenderCamera.cpp
EditorViewportClient.cpp
ViewportWindow.cpp
Editor.cpp
CameraComponent.cpp
```

### 小改 / 仅 RHI Get

```
BasePass.cpp, PresentPass.cpp, PostProcessPass.cpp
OpenGLRHI.cpp, RHIBuffers.cpp, Material.cpp, AssetManager.cpp
```

---

## 5. 接口形态前后对照

| API | 重构前 | 重构后 |
|-----|--------|--------|
| 画一帧 | `RenderSystem::Tick()` → `Pipeline.Execute()` | `SubmitSceneDraw(desc)` × N → `Tick()` → `Execute(desc)` |
| 场景 | `RenderSystem::m_RenderScene` | `Scene::GetRenderScene()` |
| 相机 | `RenderSystem::GetMainCamera()` | `SceneViewport::GetCamera()` |
| 颜色贴图 | `RenderSystem::GetSceneColorTexture()` | `SceneViewport::GetColorTexture()` |
| 视口缩放 | `RenderSystem::RequestSceneViewportResize` | `SceneViewport::RequestResize` |
| 队列 | `RenderPipeline::m_OpaqueQueue` | `SceneRenderContext::OpaqueQueue`（每 Execute 清空） |

---

## 6. 阶段依赖关系

```text
P0 ──► P1（Pipeline+Context，Editor 必须能看）
         │
         ▼
       P2（Scene 持 RenderScene，System 服务化，Editor 必须能看）
         │
         ├─► P2b（EditorViewportWindow + SceneEditingViewportWindow，Editor 必须能看）
         │
         ├─► P3（SceneEditingViewportClient 拥有 SceneViewport，Editor 必须能看）
         │
         ├─► P4（GameMainViewport，可延后）
         │
         └─► P5（MaterialPreview 窗口 + 第二 RT/Submit，主 SceneEditing 必须能看）
```

**建议实施顺序：** P0 → P1 → P2 → **P2b** → P3 →（P5 可与 P3 后并行）→ P4。

---

## 7. 风险与约束

1. **Shutdown 顺序：** 先销毁 Component/Scene，再 `RenderSystem::Shutdown`（保持现状）。
2. **同帧多次 Execute：** 共享 `ShadowResourceManager` / UBO — 顺序执行；Preview 关 Shadow 可避免争用。
3. **OpenGL 单上下文：** 不引入渲染线程；所有 Execute 仍在 `RendererTick` 同线程。
4. **不完全重构时：** 允许 P2 在 `RenderSystem::Tick` 内 **临时** 组装 Submit，但必须在 PR 说明里标注，P3 完成后删除临时路径。

---

## 8. 验收总表（发布 Material Editor 前）

| 项 | 要求 |
|----|------|
| Editor 主视口渲染 | 与重构前一致 |
| 多 Scene 数据 | 关卡 `RenderScene` + Preview 独立 `RenderScene`（P5） |
| 多 RT | 主视口 RT ≠ Preview RT（P5） |
| 无全局 Scene 在 RenderSystem | P2 后 |
| Pipeline 无成员队列 | P1 后 |
| Material IR 测试 | 不回归 |

---

## 9. 与 UE 概念速查

| UE | minEngine |
|----|-----------|
| `FScene` | `RenderScene` |
| `FRenderTarget`（视口） | `SceneRenderTarget` |
| `FSceneViewFamily` | `SceneDrawDesc` + 一次 `Execute` |
| `FSceneRenderer` 本帧数据 | `SceneRenderContext` |
| `FEditorViewportClient` | `EditorViewportClient` + `SceneViewport` |
| `UGameViewportClient` | `GameMainViewport`（P4） |
| `FPreviewScene` | Preview 专用 `RenderScene`（P5） |

---

## 10. 下一步

- **P2 / P2b / P3 已完成**（Editor 主视口 Submit + `SceneEditingViewportClient` 持 `SceneViewport`）。
- **当前：** **P5**（Material Preview 第二 RT + Submit）；主视口必须保持正常。
- **P4：** 不做（Playground 废弃）。

---

## 11. 待定事项（Editor 优先，GameRuntime 后做）

| 项 | 说明 | 优先级 |
|----|------|--------|
| **GameRuntime 视口** | `Engine`（或 Play 子系统）持有 `SceneViewport m_GameViewport`；全屏/运行时 `PresentToBackBuffer`；与 Editor 视口分离 | 低（无 Playground 需求前不实施） |
| **CameraComponent 绑定** | `SetSelfAsMainCamera` 今日经 `SceneManager::GetEditorSceneViewport()` 桥接；Game 路径应注册到 Game 视口而非 Editor | 随 GameRuntime 视口 |
| **Playground** | 仅保留废弃说明，不接入 Submit | 不实施 |

**原则：** 新视口、Submit、RT 缩放一律先在 **Editor**（`SceneEditing` / `MaterialPreview`）验证，再考虑运行时。
