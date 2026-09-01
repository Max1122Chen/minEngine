# Granite-style RDG + Frame Resource Ownership — Design Spec

## Meta
- **ID:** `RND-F07`
- **Type:** Refactor
- **Status:** Done *(Phase 1 + Phase 2 shell; bake 语义未完 → **RND-F12**)*
- **Owner:** project maintainer
- **Last updated:** 2026-08-30
- **Related:** [Implementation](./RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_IMPLEMENTATION.md), [FEATURE_REGISTRY](../FEATURE_REGISTRY.md), [ACTIVE_WORK](../ACTIVE_WORK.md), [RND-F01](./RND-F01_RENDER_GRAPH_DESIGN.md)（Manual 图；S06+ 由 F07 取代）, [RND-F06](./RND-F06_FORWARD_RENDERER_DESIGN.md), [**RND-F12**](./RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md)（**Granite 全语义复刻**；F07 仅 shell）
- **Reference source (local):** `D:\Dev\GitRepo\Granite\renderer\render_graph.hpp` / `render_graph.cpp`（MIT；对照 bake / setup_attachments / execute）

## TL;DR

帧合成 RT / 阴影图从 **Scene + ShadowResourceManager 外置持有**迁到 `RegisterExternal` / Manual 图外分配，改为 **Granite 式 RDG** 拥有。本 Feature **已完成** Phase 1（停工拆除 + 无帧 RT 泄漏）与 Phase 2 **外壳**（`add_pass` / `Bake` 骨架 / `SetupAttachments` / `IRenderPass` 五段式 / 主路径接回）。**未完成** Granite `bake()` 后半段语义（read-edge 闭包、reorder、physical pass merge、transient、automatic barrier）——该缺口由 **RND-F12** 承接；[BUG-RENDER-013](../bugs/BUG-RENDER-013.md) 为在未完成 RDG 上继续堆功能的典型暴露。

## Scope
- **In:**
  - Phase 1：帧路径停工；拆除 SceneColor/Depth、shadow atlas、post ping-pong 的外置分配
  - Phase 2a：Granite `RenderGraph` 核心骨架；Pass 声明 API；`Bake` / `SetupAttachments` / `EnqueueRenderPasses`
  - Phase 2b：主路径 Pass 接回（Scene / Post / Shadow 图节点）
  - 本 Spec + Implementation；Registry / ACTIVE_WORK；F01 S06+ 实验 Bake 产品化取消
- **Out:**
  - 完整 Vulkan transient / async compute / subpass merge / history（Granite 全量能力 → **RND-F12** 分阶段）
  - 材质 IR / Editor UI；资产 Texture/Mesh GPU 加载路径
  - Phase 1 之后继续「临时帧资源池保画面」

## Reader quick start
1. 本文件：**目标架构**与 **§3.6 类型/API 映射**
2. [Implementation](./RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_IMPLEMENTATION.md)：**切片 DoD**
3. Granite：`RenderGraph::bake` / `setup_attachments` / `RenderPassInterface`
4. 续作未完成语义：[RND-F12 Design](./RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md)
5. 代码：`ForwardRenderer.cpp`、`RenderGraph/`、`RenderPipeline/RenderPasses/*`

---

## 1) 背景与目标

### Pain
- RDG 与帧资源所有权分裂：**双轨** RT / 纹理（Scene / ShadowManager 外置 `CreateTexture`）
- `RND-F01` 实验 Bake（`a484daa`）依赖 external 注册，与 Granite 资源寿命模型不一致
- 每帧容器与 GPU 资源释放需显式；Pass 顺序易碎

### Goals
- **对齐 Granite RDG 所有权**：Pass 声明 IO → `Bake` 决定顺序 / 别名 /（目标）transient → `Execute` 只读 physical
- **消灭帧路径外置 RT**：除 swapchain / 编辑器特殊视图外，Scene 色深与 shadow atlas 由图创建
- **分阶段交付**：Phase 1 允许黑屏；Phase 2 先外壳再接回画面

### Success（已达成部分）
- Phase 1：**零**帧路径 SceneColor/Depth / shadow / post 外置分配
- Phase 2 shell：`ForwardRenderer` 主路径 Pass 走 `SetupDependencies` → `Bake` → `SetupAttachments` → `Enqueue`；无 `RegisterExternal`
- Phase 2b：Scene / Post / Shadow 图节点接回；`test render-graph` + smoke 通过

### 未达成（→ RND-F12）
- Granite 完整 `bake()` 语义与 `build_physical_barriers`
- Scene pass 对 shadow atlas 的 **read dependency** 闭包
- 删除 `ForceIncludePass` / `BuildShadowResourceFingerprint` 等 Renderer 侧补丁

---

## 2) 现状对照（F07 完成时）

| 组件 | F07 完成后行为 | 与 Granite 差距 |
|------|----------------|-----------------|
| `SceneRenderTarget` | Publish 图拥有的 SceneColor/Depth | 视图句柄；非图内资源 |
| `ShadowResourceManager` | 已删除持纹理；F08 迁入图 | — |
| `ForwardRenderer` | 构图 + `Bake()` + `BindGraphShadowTextures` + `BuildSceneSet1` + `Enqueue` | set1 仍在图外；`ForceInclude` shadow |
| `RenderGraph/` | 声明 + 简化 `Bake` + `SetupAttachments` | 无 reorder / merge / transient / barrier |
| 资产 / Mesh | 不变 | 本 Feature 范围外 |

历史：`a484daa` experimental Bake 已废弃。

---

## 3) 方案

### 3.1 阶段划分

```text
Phase 1  停工拆除 + 零帧 RT 分配
    ↓
Phase 2a  Granite 式 RDG 外壳（S04–S06）
    ↓ bake / physical / setup_attachments / execute 骨架
Phase 2b  主路径 Pass 接回（S07–S09）
    ↓
RND-F12   Granite bake 语义补全（read edge / barrier / transient / …）
```

**纪律：** Phase 1 期间 Renderer 不引入替代帧资源池；Phase 2 化用 Granite 结构，不在旧 Manual builder 上打补丁。

### 3.2 Phase 1 — 停工与拆除

**目标：** 帧路径不再为合成 RT 分配 GPU 纹理。

**Touch（见 Implementation S01–S03）：**
- `SceneRenderTarget` 帧路径 `Initialize`/`Resize` 不创建 SceneColor/Depth
- `ShadowResourceManager::Ensure*` / Acquire 帧纹理创建移除
- `ForwardRenderer` post ping-pong 外置创建移除
- `Execute` 早退或仅最小竖切；Present 不依赖外置 SceneColor

**DoD：** 无帧路径 `CreateTexture` 用于 Scene/Shadow/Post；smoke 非渲染子集绿。

### 3.3 Phase 2 — Granite 式 RDG

#### 3.3.1 API 映射（Granite → minEngine）

| Granite | 含义 | minEngine（F07 落地） |
|---------|------|------------------------|
| `RenderPassInterface` | `setup_dependencies` → bake 后 `setup` → 每帧 `enqueue_prepare` / `build_render_pass` | `IRenderPass` 同阶段命名 |
| `RenderPass::add_*` | 声明 IO + `AttachmentInfo` | `RenderPass::AddColorOutput` 等；**不**持有 `RHITexture*` |
| `RenderGraph::bake` | filter → physical → merge → transient → barriers | **仅** traverse + `build_physical_resources`（简化） |
| `setup_attachments` | 创建/复用物理资源；swapchain alias | 已落地；按 dims 比对复用 |
| `get_physical_texture_resource` | Bake 后查询 | `GetPhysicalTexture` / `TryGetPhysicalTexture` |

对照源码：`D:\Dev\GitRepo\Granite\renderer\render_graph.cpp` `bake()` 约 L2993 起。

#### 3.3.2 化用原则

1. **先拆外置 RT**，再让 Bake 成为唯一分配点
2. **禁止**帧路径 `CreateTexture` + `RegisterExternal` 喂主路径
3. **按 Granite 阶段表**实现或显式 Deferred（注释 + RND-F12 承接）
4. **不**在旧 experimental Bake 上续写产品功能
5. Granite MIT 源码为权威参考；Progress/Design 记录 minEngine 裁剪

#### 3.3.3 接回顺序（S07–S09）

1. 单 color RT clear/present 竖切
2. SceneColor/Depth + Sky/Opaque/Translucent
3. Post ping-pong
4. Shadow 图节点（F08 深化所有权）
5. 删除 `RegisterExternal` 主模型

### 3.4 关联 Feature

| ID | 关系 |
|----|------|
| `RND-F01` | Manual S0–S05；**S06+ 由 F07 取代** |
| `RND-F06` | `ForwardRenderer` 职责分离；与 F07 并行 |
| `RND-F08` | Shadow atlas 图所有权尾（接 F07 S08） |
| `RND-F12` | **F07 Phase 2 bake 语义续作** |
| `RND-F03` / `RND-F05` | RHI 基础；VK barrier 细节可跟 F12 |

### 3.5 目标架构

```text
ForwardRenderer（策略：Pass 列表、UBO、场景收集）
        │
        ▼
RenderGraph（Granite-style）
  AddPass / AttachmentInfo 声明
  Bake() → physical resources + pass order
  SetupAttachments(rhi, swapchain?)
  EnqueueRenderPasses → IRenderPass::BuildRenderPass + GetPhysicalTexture
        │
        ▼
RHI CommandList / 后端
```

Scene 呈现通过 `SceneRenderTarget::PublishGraphColorTexture` 持有图输出句柄，**不**拥有 GPU 分配权。

### 3.6 类型与 API（Phase 2 落地子集）

> 完整结构见代码 `RenderGraph/`、`RDGTypes.h`。以下为 F07 定稿映射；Deferred 项由 RND-F12 补齐。

#### 3.6.1 核心类型

| Granite | minEngine |
|---------|-----------|
| `AttachmentInfo` | `RDGAttachmentInfo` |
| `ResourceDimensions` | `RDGResourceDimensions` |
| `RenderResource` | `RDGResource` |
| `RenderTextureResource` | `RDGTextureResource` |
| `RenderPass`（图节点） | `RenderPass` |
| `RenderPassInterface` | `IRenderPass` |
| `RenderGraph` | `RenderGraph` |

已删除：`RenderPassBuilder`、`RenderGraphFrameResources`、`RegisterExternal` 主路径。

#### 3.6.2 `RDGSizeClass`

- `Absolute` — shadow atlas 等固定分辨率
- `SwapchainRelative` — SceneColor/Depth
- `InputRelative` — **Deferred → RND-F12**

#### 3.6.3 `IRenderPass` 生命周期

| 阶段 | 时机 | 允许 |
|------|------|------|
| `SetupDependencies` | 每次 `Bake()` 前 | 仅声明 IO |
| `Setup` | `Bake()` 后一次 | PSO/采样器；不分配帧 RT |
| `Prepare` | 每帧 enqueue 前 | CPU 侧 draw 列表 |
| `BuildRenderPass` | 每帧 GPU 录制 | `graph.GetPhysicalTexture` + RHI |

#### 3.6.4 `RenderGraph::Bake`（F07 已实现 vs Granite 全量）

| 步骤 | Granite | F07 | RND-F12 |
|------|---------|-----|---------|
| SetupDependencies | ✓ | ✓ | — |
| validate_passes | ✓ | ✓ | — |
| traverse_dependencies | ✓ 全 IO 类型 | 部分（texture/depth input） | 闭合 generic read |
| reorder_passes | ✓ | ✗ | S02 |
| build_physical_resources | ✓ | ✓ 简化 | alias 增强 |
| build_physical_passes | ✓ merge | ✗ 1:1 | S05 可选 |
| build_transients | ✓ | ✗ | S04 |
| build_render_pass_info | ✓ | ✗ | 随 merge |
| build_physical_barriers | ✓ | ✗ | S03 |
| Pass Setup(rhi) | ✓ | ✓（SetupAttachments 内） | — |

#### 3.6.5 示例：Opaque 应声明 shadow read（F12 目标）

```cpp
void BasePass::SetupDependencies(RenderPass& self, RenderGraph& graph)
{
    self.AddColorOutput(kRDGSceneColor, MakeSceneColorAttachment());
    self.SetDepthStencilOutput(kRDGSceneDepth, MakeSceneDepthAttachment());
    self.AddTextureInput(kRDGDirShadowAtlas);   // F12：consumer 拉 producer
    // spot/point atlas 同理
}
```

F07 接回时 **未** 添加上述 read edge；依赖 `ForceIncludePass("Shadow.*")` — 此为 RND-F12 首要删除项。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 保留 external + 实验 Bake | 改动小 | 非 Granite | **拒绝** |
| B. Phase 1 永久停工 | 风险低 | 无画面 | **仅过渡** |
| C. Phase 1 停工 → Phase 2 Granite 外壳 → F12 语义 | 可验证、可回滚 | 两阶段 Feature | **选用** |
| D. 继续在 Renderer 堆 fingerprint / 手工 enqueue | 短期可能见效 | 非架构解；已证伪 BUG-013 | **拒绝** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Phase 1 黑屏过长 | 开发体验 | 尽快 S05 竖切 |
| Bake 简化导致 VK 同步问题 | 阴影/后处理偶发错 | **RND-F12**；不以 binding patch 代替 |
| GL 与 VK barrier 差异 | 双后端行为分叉 | F12 在 RHI 抽象层统一；GL 可 no-op barrier |
| F07 误标 Done 掩盖缺口 | 后续 bug 被当 shader 问题 | 2026-08-30 修订 Meta；登记 F12 |

---

## 6) 验收标准

### Phase 1（Done）
- [x] 帧路径零外置 Scene/Shadow/Post RT
- [x] `ForwardRenderer` 停工或最小竖切
- [x] smoke 约定子集 PASS

### Phase 2 shell（Done）
- [x] Granite 式声明 API + `Bake` / `SetupAttachments` / `Enqueue`
- [x] 主路径无 `RegisterExternal`
- [x] Scene/Post/Shadow 图节点接回
- [x] `test render-graph` PASS

### Phase 2 bake 语义（→ RND-F12）
- [ ] read-edge 闭包；删除 `ForceInclude` shadow
- [ ] 删除 `BuildShadowResourceFingerprint`
- [ ] pass 间 layout barrier（VK）
- [ ] BUG-RENDER-013 回归

---

## 7) Status note

**Done（外壳）** — S01–S09 按 Implementation 关账。  
**续作：** [RND-F12](./RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md) — **尽可能完整复刻 Granite RDG 全部语义**（Phase A–D）；F07 仅外壳。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-02 | 初稿；Phase 1/2 划分；对照 Granite |
| 2026-08-02 | §3.6 API 映射；Status In Progress → Done（当时认定） |
| 2026-08-30 | **恢复 UTF-8 正文**（原文件提交时编码已损）；修订 Meta：bake 语义未完 → RND-F12；§6 拆分 shell vs 语义验收 |
