# Hand-Pass Probe Renderer — Design Spec

## Meta
- **ID:** `RND-F13`
- **Type:** Feature *(diagnostic / experimental)*
- **Status:** **Draft** — pending maintainer approval
- **Owner:** project maintainer
- **Last updated:** 2026-08-30
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md), [ACTIVE_WORK](../ACTIVE_WORK.md), [RND-F12](./RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md), [BUG-RENDER-013](../bugs/BUG-RENDER-013.md), [BUG-RENDER-010](../bugs/BUG-RENDER-010.md)
- **Implementation:** *待审批后* — `RND-F13_HAND_PASS_PROBE_RENDERER_IMPLEMENTATION.md`

## TL;DR

新增 **对照实验用** `HandPassProbeRenderer`（名称待定）：保留现有 `ForwardRenderer` + RDG 主线不动；新 Renderer **仅手写组织** `ShadowPass → BasePass → PresentPass`，不走 `RenderGraph::Bake/Enqueue/PermanentOutput`。

若 VK 手写路径与 GL 一致，而 `ForwardRenderer`+RDG 仍异常 → **坐实 BUG-RENDER-013 根因在 RDG 图语义层**（非 shadow shader / clip-space / 基础 RHI）。

## Scope
- **In:**
  - 独立 `SceneRenderer` 实现；复用现有 `ShadowPass` / `BasePass` / `PresentPass` 与 `EngineSceneBindingSets`
  - 手写帧资源（SceneColor、SceneDepth、DirShadowAtlas）分配与 pass 顺序、layout transition
  - Editor / CLI 切换入口（如 `--renderer handpass` 或等效配置）
  - 固定实验场景：`test`、dir shadow 为主；GL/VK 对照目视
- **Out:**
  - 替换或删除 `ForwardRenderer` / `RenderGraph`
  - SkyBox、PostProcess、Translucent、多光源 full graph（后续 slice 可选扩展）
  - 修复 RDG 本身（属 **RND-F12**）；修复 CSM 质量（属 **BUG-RENDER-010**）
  - 新产品级 Renderer；实验代码明确 Tier C，可随时删除

## Reader quick start
1. 本文件：实验目的、边界、验收
2. 审批通过后写 Implementation Plan（切片 + DoD）
3. 代码入口（计划）：`RenderPipeline/HandPassProbeRenderer.*`、`RenderSystem` 工厂注册

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
  └─ HandPassProbeRenderer     → 若 VK ≈ GL → RDG 为差分变量
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

**实验配置（工作区临时，commit `3fed4ef`）：** `MAX_*_SHADOW_MAPS=0`、`MAX_CASCADES=1`、`DIR_SHADOW_FORCE_CASCADE=0` — 对照 Renderer **首版应在恢复生产常量前或与之对齐的实验矩阵中明确声明**。

---

## 3) 方案

### 3.1 架构

```text
RenderSystem
  ├─ ForwardRenderer          (现有，RDG)
  └─ HandPassProbeRenderer    (新建，实验)
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
| `DirShadowAtlas` | ProbeRenderer 持有 `RHITextureRef` | `Texture2DArray` 或单 cascade `Texture2D`（与实验矩阵一致） |
| `SceneColor` / `SceneDepth` | ProbeRenderer 或 `SceneRenderTarget` 内部分配 | 与 Forward 同分辨率策略 |
| Swapchain / viewport | `SceneRenderTarget` publish | Present 复用现有路径 |

不调用 `RenderGraph::SetupAttachments`；shadow 纹理在 shadow pass 前绑定到 `ShadowPass` / set1。

### 3.3 API / 切换

| 入口 | 行为 |
|------|------|
| `--renderer handpass`（或 `EngineConfig` 枚举） | `RenderSystem` 构造 `HandPassProbeRenderer` |
| 默认 | 仍为 `ForwardRenderer` |

文档与代码注释标明 **EXPERIMENTAL / DIAGNOSTIC ONLY**。

### 3.4 与 BUG-010 / F12 关系

| 议题 | 本 Feature | 负责 Feature |
|------|------------|--------------|
| CSM 多影 / layer-matrix | 实验时可沿用单 cascade 基线 | BUG-RENDER-010 |
| RDG PermanentOutput / 满图 | 对照 Renderer **故意绕过** | RND-F12 |
| 多光源 full graph | 首版 Out；扩展 slice 可选 | RND-F12 / BUG-013 |

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 新 Probe Renderer（本方案） | 隔离清晰；不污染 Forward | 多一个类 | **选用** |
| B. `ForwardRenderer` 内 `if (bypassRdg)` | 改动集中 | 长期难维护；难读 | 拒绝 |
| C. 仅 RenderDoc 对比 | 零代码 | 不能自动化回归；难证 enqueue 语义 | 作辅助，不替代 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 实验 Renderer 引入新 bug | 假阴性/假阳性 | 最大化复用 Pass；与 Forward 共享 UBO/set 路径 |
| 与临时 isolation 常量纠缠 | 结论混淆 | Implementation 首 slice 写清实验矩阵；恢复常量后复测 |
| 范围蔓延（加 Sky/Post） | 延期 | 严格 Out；仅 Shadow+Base+Present |
| 实验代码被当成正式路径 | 误用 | Registry Status + 代码命名 + 文档 Tier C |

---

## 6) 验收标准（Design Done ≠ Impl Done）

### 6.1 对照实验结论（本 Feature 核心）

- [ ] **同一 `test` 场景、同一实验矩阵**（建议先 dir-only / 单 cascade）：
  - GL：`HandPassProbeRenderer` 阴影目视 OK
  - VK：`HandPassProbeRenderer` 阴影目视 **≈ GL**
  - VK：`ForwardRenderer`+RDG 仍异常（或记录差异）
- [ ] 结论写入 [BUG-RENDER-013](../bugs/BUG-RENDER-013.md)（RDG 坐实 / 未坐实）

### 6.2 工程

- [ ] `HandPassProbeRenderer` 可切换、可编译；不影响默认 Forward 路径
- [ ] 无 `RenderGraph` 依赖（grep 验证）
- [ ] Implementation Plan 与 PROGRESS_LOG 条目（审批后）

---

## 7) Status note

**Draft** — 待 maintainer 审批设计后进入 Implementation Plan；**不启动编码**直至批准。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-30 | 初稿：对照实验 Renderer；动机来自 BUG-010 CSM 坐实 + BUG-013 RDG 假设 |
