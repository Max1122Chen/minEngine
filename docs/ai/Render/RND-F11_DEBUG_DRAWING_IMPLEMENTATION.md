# DebugDrawing — Implementation Plan

## Meta
- **ID:** `RND-F11`
- **Type:** Implementation Plan
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-01
- **Branch:** `feat/debug-drawing`
- **Design:** [RND-F11_DEBUG_DRAWING_DESIGN.md](./RND-F11_DEBUG_DRAWING_DESIGN.md)

## TL;DR

共 **4 个切片**（S01–S04）。当前 **S02 Planned**（S01 Done）。

**架构约束（与 Design §4 / §11 一致）：**
- **提交在 Renderer 外**：Editor / 测试在 `SubmitSceneDraw` **之前** 调用 `DebugDraw::` / `PhysicsDebugDraw::`
- **ForwardRenderer 只消费**：注册 `Scene.Debug` pass；**不** include Physics、**不**调用 `PhysicsDebugDraw`
- **`SceneDrawDesc` 不增** gameplay `Scene*` 字段
- **`PhysicsDebugDraw`** 位于 `Runtime/Function/Physics/`

**阻塞项：** 无。禁止改 `ShadowPass` / `ManualRenderer`。

## Scope
- **In:** Design §3–§12 所述 Runtime/Debug、Render Pass、Shader、Physics 适配、Editor flag
- **Out:** ManualRenderer DebugPass；`GameplayScene`；ForwardRenderer→Physics 耦合

## Reader quick start
1. [Design Spec](./RND-F11_DEBUG_DRAWING_DESIGN.md) — 架构、依赖规则、帧时序
2. 下文 §1 切片总览
3. §2 切片详情 — 文件清单、DoD、验证

---

## 1) 切片总览

| Slice ID | 标题 | 状态 | 验证 |
|----------|------|------|------|
| RND-F11-S01 | Debug 核心 + Pass + RDG 竖切 | Done | GL+VK 轴线目视 |
| RND-F11-S02 | Collider wireframe | Planned | Editor collider 目视 |
| RND-F11-S03 | Contact + LineTrace | Planned | `physics-contact` + trace 目视 |
| RND-F11-S04 | Debug draw toggle | Planned | Editor toggle |

---

## 2) 切片详情

### RND-F11-S01 — Debug 核心 + Pass + RDG 竖切

**Goal：** `DebugDraw` API + `DebugDrawPass` + RDG；Editor 可见世界空间线段；**零 Physics 依赖**。

#### Touch（新建）

| 路径 | 说明 |
|------|------|
| `minEngine/Shaders/DebugDraw.vert` / `.frag` | 无光照；`PerFrame.ViewProj` |
| `Runtime/Function/Debug/DebugDrawTypes.h` | `DebugVertex`、enums（**无 RHI 类型**） |
| `Runtime/Function/Debug/DebugGeometry.h/.cpp` | `AppendLine` / `AppendPointCross` / `AppendBoxWireframe` |
| `Runtime/Function/Debug/DebugDrawService.h/.cpp` | 队列、`BuildFrameGeometry`、`ClearFrameQueues` |
| `Runtime/Function/Debug/DebugDraw.h/.cpp` | `DebugDraw::Line` / `Point` / `Box` |
| `RenderPipeline/RenderPasses/DebugDrawPass.h/.cpp` | 动态 VB、PSO、Prepare/Draw |
| `SceneDrawDesc.h` | `EnableDebugDraw` flag **only** |

#### Touch（修改）

| 路径 | 说明 |
|------|------|
| `ForwardRenderer.h/.cpp` | 成员 `m_DebugDrawPass`；RDG 条件 `AddPass("Scene.Debug")`；**仅** include `DebugDrawPass.h` |
| `SceneEditingViewportClient.cpp` | flags 加 `EnableDebugDraw`；**S01 smoke：SubmitSceneDraw 前** 提交 RGB 轴线 |

**S01 smoke（在 Editor 提交点，非 ForwardRenderer）：**

```cpp
// SceneEditingViewportClient — SubmitSceneDraw 之前，仅 S01
if (HasSceneDrawFlag(flags, SceneDrawFlags::EnableDebugDraw))
{
    DebugDraw::Line({0,0,0}, {5,0,0}, {1,0,0,1});
    DebugDraw::Line({0,0,0}, {0,5,0}, {0,1,0,1});
    DebugDraw::Line({0,0,0}, {0,0,5}, {0,0,1,1});
}
RenderSystem::Get().SubmitSceneDraw(desc);
```

S02 移除上述 smoke，改为 `PhysicsDebugDraw::SubmitScene`。

#### Prepare 时序（必须）

```text
DebugDrawPass::Prepare:
  BuildFrameGeometry()
  Upload VB
  ClearFrameQueues()    // Build 之后，非开头
```

#### DoD

- [ ] `DebugDrawService` 无 RHI / Physics / Scene include
- [ ] `DebugDrawPass` 无 Physics include
- [ ] `ForwardRenderer` 无 Physics / `PhysicsDebugDraw` / `DebugDraw.h` include（仅 Pass）
- [ ] `SceneDrawDesc` 无 gameplay 字段
- [ ] `ManualRenderer`、`ShadowPass` 未改
- [ ] Editor GL+VK 可见轴线且深度遮挡正确
- [ ] `verify.ps1` 通过

#### Verify

```powershell
cmake --build minEngine/build --target minEngine Editor
.\scripts\verify.ps1
minEngine\bin\Editor.exe --rhi opengl --project ..\MyMEProject\MyMEProject.meproject
minEngine\bin\Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject
```

---

### RND-F11-S02 — Collider wireframe

**Goal：** `PhysicsDebugDraw` 遍历 gameplay `Scene`；移除 S01 轴线 smoke。

#### Touch（新建）

| 路径 | 说明 |
|------|------|
| `Runtime/Function/Physics/PhysicsDebugDraw.h/.cpp` | `SubmitScene`、channel 配色 |
| `DebugGeometry` 扩展 | `AppendSphereWireframe`、`AppendCapsuleWireframe` |

#### Touch（修改）

| 路径 | 说明 |
|------|------|
| `SceneEditingViewportClient.cpp` | 移除 S01 轴线；在 `SubmitSceneDraw` **前** 调用 `PhysicsDebugDraw::SubmitScene` |
| `DebugDrawService` | Sphere/Capsule 命令队列 |

**Editor 提交点（示意）：**

```cpp
Scene* scene = sceneEditor->GetActiveScene();
const SceneDrawFlags flags = ... | SceneDrawFlags::EnableDebugDraw;

if (scene && HasSceneDrawFlag(flags, SceneDrawFlags::EnableDebugDraw))
{
    PhysicsWorld& world = PhysicsSystem::Get().GetOrCreateWorld(scene);
    PhysicsDebugDraw::SubmitScene(*scene, world, PhysicsDebugDraw::GetOptions());
}

const SceneDrawDesc desc = GetSceneViewport().BuildDrawDesc(flags);
// desc.Scene 仍为 renderScene — 不填 gameplay Scene
RenderSystem::Get().SubmitSceneDraw(desc);
```

**不修改：** `ForwardRenderer`、`SceneDrawDesc`（除已有 flag）。

#### DoD

- [ ] Box / Sphere / Capsule wireframe 目视正确
- [ ] Channel 颜色可区分
- [ ] S01 轴线 smoke 已移除
- [ ] Thumbnail / Preview 视口无 collider 线（未开 `EnableDebugDraw`）
- [ ] `physics-smoke` / `physics-shapes` 通过

---

### RND-F11-S03 — Contact + LineTrace

**Goal：** 扩展 `PhysicsContactEvent`；contact 点/法线；`SubmitLineTrace`。

#### Touch

| 路径 | 说明 |
|------|------|
| `PhysicsTypes.h` | `Position`、`Normal` |
| `PhysicsWorld.cpp` | Jolt manifold 填充 |
| `PhysicsDebugDraw.cpp` | `SubmitContacts`、`SubmitLineTrace` |
| `PhysicsContactTest.cpp` | 更新断言 |

**调用方：** 测试直接调 `SubmitLineTrace`；Editor contact 由 `SubmitScene` 内读 `GetContactEvents()`。

#### DoD

- [ ] Contact Begin：点 + 法线可见
- [ ] `physics-contact` / `physics-linetrace` 通过
- [ ] 仍无 ForwardRenderer→Physics 耦合

---

### RND-F11-S04 — Debug draw toggle

**Goal：** 可关闭 Physics debug 绘制。

#### Touch

| 路径 | 说明 |
|------|------|
| `PhysicsDebugDraw` | `Options` + `SetDrawCollidersEnabled` |
| `SceneEditor::RegisterCommands` 或 viewport | `debug.draw physics` toggle |

关闭时：`SubmitScene` 早退；或 Editor 不再调用 `SubmitScene`（二选一，推荐早退 + 仍开 pass）。

#### DoD

- [ ] Toggle 可开关 collider wireframe
- [ ] 默认 on（仅 `SceneEditingViewportClient`）

---

## 3) 依赖顺序

```text
S01 → S02 → S03 → S04（S04 可与 S03 末尾并行）
```

---

## 4) 工程清单

### 4.1 新建文件

```text
minEngine/Shaders/DebugDraw.vert
minEngine/Shaders/DebugDraw.frag
Runtime/Function/Debug/DebugDrawTypes.h
Runtime/Function/Debug/DebugGeometry.{h,cpp}
Runtime/Function/Debug/DebugDrawService.{h,cpp}
Runtime/Function/Debug/DebugDraw.{h,cpp}
Runtime/Function/Physics/PhysicsDebugDraw.{h,cpp}    ← Physics 域，非 Debug/
Runtime/Function/Render/RenderPipeline/RenderPasses/DebugDrawPass.{h,cpp}
```

### 4.2 修改文件

```text
SceneDrawDesc.h                    // EnableDebugDraw only
ForwardRenderer.{h,cpp}            // DebugDrawPass member + RDG
SceneEditingViewportClient.cpp     // flag + 提交点（smoke / PhysicsDebugDraw）
PhysicsTypes.h / PhysicsWorld.cpp  // S03
PhysicsContactTest.cpp             // S03
SceneEditor.cpp                    // S04 optional
```

### 4.3 依赖 grep 验收

```powershell
# Debug 核心无 RHI
rg "#include.*RHI" minEngine/minEngine/src/Runtime/Function/Debug/

# Pass 无 Physics
rg "#include.*Physics" minEngine/minEngine/src/Runtime/Function/Render/RenderPipeline/RenderPasses/DebugDrawPass.*

# ForwardRenderer 不感知 Physics / 不入队 debug 内容
rg "PhysicsDebugDraw|PhysicsWorld|DebugDraw::" minEngine/minEngine/src/Runtime/Function/Render/RenderPipeline/ForwardRenderer.*

# ManualRenderer 无 Debug
rg "DebugDraw" minEngine/minEngine/src/Runtime/Function/Render/RenderPipeline/ManualRenderer.*
```

期望：ForwardRenderer 仅匹配 `DebugDrawPass` 成员名（若有 grep 命中需审查是否为 include/调用）。

---

## 5) Feature Done

- [ ] Design §15 验收项
- [ ] §4.3 grep 验收通过
- [ ] S01–S04 Done
- [ ] `FEATURE_REGISTRY` / `PROGRESS_LOG` / `ACTIVE_WORK` 更新

---

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 初稿：S01–S04 |
| 2026-09-01 | 解耦修订：提交在 Editor；无 GameplayScene；PhysicsDebugDraw → Physics/；队列 Clear 时序 |
