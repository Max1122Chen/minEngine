# ManualRenderer — Design Spec

## Meta
- **ID:** `RND-F13`
- **Type:** Feature *(diagnostic / experimental)*
- **Status:** **Done** *(Reference — `--renderer manual` retained for diagnosis)*
- **Owner:** project maintainer
- **Last updated:** 2026-08-31
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md), [ACTIVE_WORK](../ACTIVE_WORK.md), [RND-F12](./RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md), [BUG-RENDER-013](../bugs/BUG-RENDER-013.md), [BUG-RENDER-010](../bugs/BUG-RENDER-010.md)
- **Implementation:** [RND-F13_MANUAL_RENDERER_IMPLEMENTATION.md](./RND-F13_MANUAL_RENDERER_IMPLEMENTATION.md)

## TL;DR

新增 **对照实验用** `ManualRenderer`：保留现有 `ForwardRenderer` + RDG 主线不动；新 Renderer **仅手写组织** `ShadowPass → BasePass → PresentPass`，不走 `RenderGraph::Bake/Enqueue/PermanentOutput`。

若 VK 手写路径与 GL 一致，而 `ForwardRenderer`+RDG 仍异常 → **坐实 BUG-RENDER-013 根因在 RDG 图语义层**（非 shadow shader / clip-space / 基础 RHI）。

## Scope
- **In:**
  - 独立 `SceneRenderer` 实现；复用现有 `ShadowPass` / `BasePass` / `PresentPass` 与 `EngineSceneBindingSets`
  - 手写帧资源（SceneColor、SceneDepth、DirShadowAtlas）分配与 pass 顺序、layout transition
  - Editor / CLI 切换入口：`--renderer manual`（`handpass` 别名）
  - 固定实验场景：`test`、dir shadow 为主；GL/VK 对照目视
- **Out:**
  - 替换或删除 `ForwardRenderer` / `RenderGraph`
  - SkyBox、PostProcess、Translucent、多光源 full graph（后续 slice 可选扩展）
  - 修复 RDG 本身（属 **RND-F12**）；修复 CSM 质量（属 **BUG-RENDER-010**）
  - 新产品级 Renderer；实验代码明确 Tier C，可随时删除

## Reader quick start
1. 本文件：实验目的、边界、验收
2. [Implementation Plan](./RND-F13_MANUAL_RENDERER_IMPLEMENTATION.md)（切片 + DoD）
3. 代码入口：`RenderPipeline/ManualRenderer.*`、`RenderSystem` 工厂注册

---

## 1) 背景与目标

### 1.1 问题

| 已坐实 | 来源 |
|--------|------|
| Dir-only + 单 cascade → 多影消失，剩 1 影（可能偏浅） | BUG-RENDER-010 级联实验（2026-08-30） |
| Full shadow graph + RDG → VK 多光源 shadow 异常；S01–S06 稳定性改善但视觉未恢复 | BUG-RENDER-013 / RND-F12 |
| Dir shadow 算法与 TD-025 clip-space 在隔离下可工作 | 用户 dir-only 验证 |

**未坐实：** 异常是否 **仅** 由 RDG 图组织（Bake、PermanentOutput、barrier 时机）导致，而非 RHI/binding/UBO 的共用问题。

### 1.2 目标

提供 **最小对照实验场地**：

```text
同一套 Pass 实现 + 同一套 shader/UBO
  ├─ ForwardRenderer + RDG     → VK 异常（基线）
  └─ ManualRenderer            → 若 VK ≈ GL → RDG 为差分变量
```

### 1.3 非目标

- 不重做 ForwardRenderer
- 不以此 Renderer 作为长期产品路径
- 不在本 Feature 内完成 F12 的 Granite 语义修复

---

## 2) 现状

| 组件 | 角色 |
|------|------|
| `ForwardRenderer` | 场景收集、UBO、`m_FrameRenderGraph` Bake/Enqueue |
| `RenderGraph` | PermanentOutput、read edge、pass stack、SetupAttachments |
| `ShadowGraphPass` | RDG 包装 `ShadowPass`；`SetupDependencies` 永久声明 depth writer |
| `BasePass` / `PresentPass` | 已实现；依赖 graph physical 或 scene target |
| `ManualRenderer` | **S01 Done** — 继承 Forward 帧逻辑，手写 pass 顺序 |

**实验配置（工作区临时）：** `MAX_*_SHADOW_MAPS=0`、`MAX_CASCADES=1`、`DIR_SHADOW_FORCE_CASCADE=0` — 对照实验前在 bug note 写明矩阵。

---

## 3) 方案

### 3.1 架构

```text
RenderSystem
  ├─ ForwardRenderer          (现有，RDG)
  └─ ManualRenderer           (新建，实验)
        │
        ├─ 自有：帧 RT 池（非 RDG physical）
        ├─ 复用：ShadowPass, BasePass, PresentPass
        ├─ 复用：EngineSceneBindingSets, EnginePipelineLayouts
        └─ Execute 伪代码：
             CollectScene + ShadowRequests
             for each shadow cmd → ShadowPass (manual Begin/End + transition)
             transition shadow atlas → shader read
             BasePass (SceneColor + SceneDepth)
             PresentPass (→ swapchain / SceneRenderTarget)
```

**与 RDG 的唯一 intentional 差分：** pass 由 C++ 显式顺序调用，无 Bake、无 `PermanentOutput`、无 `AddSceneLitShadowTextureInputs` 闭包。

### 3.2 帧资源（Adapter）

| 资源 | 分配方 | 说明 |
|------|--------|------|
| `DirShadowAtlas` | ManualRenderer 持有 `RHITextureRef` | `Texture2DArray`，层数 = `MAX_CASCADES` |
| `SceneColor` / `SceneDepth` | ManualRenderer | 与 viewport 同分辨率 |
| Swapchain / viewport | `SceneRenderTarget` publish | Present 复用现有路径 |

### 3.3 API / 切换

| 入口 | 行为 |
|------|------|
| `--renderer manual`（`handpass` 别名） | `RenderSystem` 构造 `ManualRenderer` |
| 默认 | 仍为 `ForwardRenderer` |

文档与代码注释标明 **EXPERIMENTAL / DIAGNOSTIC ONLY**。

### 3.4 与 BUG-010 / F12 关系

| 议题 | 本 Feature | 负责 Feature |
|------|------------|--------------|
| CSM 多影 / layer-matrix | 实验时可沿用单 cascade 基线 | BUG-RENDER-010 |
| RDG PermanentOutput / 满图 | 对照 Renderer **故意绕过** | RND-F12 |
| 多光源 full graph | 首版 Out；扩展 slice 可选 | RND-F12 / BUG-013 |

---

## 4) 验收标准

### 4.1 对照实验结论（核心）

- [ ] **同一 `test` 场景、同一实验矩阵**：
  - GL：`ManualRenderer` 阴影目视 OK
  - VK：`ManualRenderer` 阴影目视 **≈ GL**
  - VK：`ForwardRenderer`+RDG 仍异常（或记录差异）
- [ ] 结论写入 [BUG-RENDER-013](../bugs/BUG-RENDER-013.md)

### 4.2 工程

- [x] `ManualRenderer` 可切换、可编译；不影响默认 Forward 路径
- [x] 无 `RenderGraph` 依赖（`ManualRenderer.cpp`）
- [x] Implementation Plan 与 PROGRESS_LOG 条目

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-30 | 初稿（HandPassProbeRenderer） |
| 2026-08-30 | 重命名 ManualRenderer；S01 实现 + CLI |
