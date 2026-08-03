# Shadow Map Graph Ownership — Design Spec

## Meta
- **ID:** `RND-F08`
- **Type:** Refactor
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-02
- **Related:** [Implementation](./RND-F08_SHADOW_GRAPH_OWNERSHIP_IMPLEMENTATION.md), [FEATURE_REGISTRY](../FEATURE_REGISTRY.md), [ACTIVE_WORK](../ACTIVE_WORK.md), [RND-F07](./RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md), **TD-020**（已关闭）

## TL;DR

F07 已让 Scene/Post 由图 `Bake`→`SetupAttachments` 拥有；阴影 depth 仍由 `ShadowResourceManager::Ensure*` 分配，`ShadowGraphPass` 只 ForceInclude。本 Feature 把 **Directional / Spot / Point** 阴影纹理迁入 RDG（Absolute + 正确 Dimension），并调整 `ForwardRenderer::Execute` 顺序，使 **SetupAttachments 先于 `BuildSceneSet1`**。完成后删除 Manager 的纹理所有权。

## Scope
- **In:** 阴影 RT 图声明与创建；handle 绑定；Execute 相位重排；删除 `Ensure*` / Manager 持有的 `RHITextureRef`；关闭 TD-020。
- **Out:** 阴影算法/CSM 质量；spot atlas 打包；VK transient；改采样 shader 语义（仍走现有 Set1 SRV）。

## Reader quick start
1. 本文件：目标结构与删除列表
2. [Implementation](./RND-F08_SHADOW_GRAPH_OWNERSHIP_IMPLEMENTATION.md)：切片
3. 代码：`ShadowGraphPass.*`、`ForwardRenderer::Execute`、`ShadowResourceManager.*`、`RenderGraph::MakeCreateDesc`

---

## 1) 背景与目标

**Pain：** 双轨所有权（图拥有场景色、Manager 拥有阴影）阻碍统一寿命与后续 VK/transient；F07 已把此路径记为 TD-020。

**Done 长什么样：**
- 所有阴影 depth 仅由图物理槽创建/复用
- `ShadowResourceHandle.Texture` 在 SetupAttachments 之后从 `GetPhysicalTextureShared` 填充
- `ShadowResourceManager` 不再 `RHICreateTexture*`；可缩成 metadata/slot 辅助或删除持纹理成员
- 黄金场景阴影视觉与今相当

## 2) 现状

| 组件 | 行为 |
|------|------|
| `ShadowResourceManager` | `Acquire*` → `Ensure*` → `RHICreateTexture2D`（2DArray / 2D / Cube） |
| `ShadowGraphPass` | `SetupDependencies` 空；ForceInclude；渲染用 command.Handle.Texture |
| `ForwardRenderer::Execute` | `BuildShadowDrawCommands`（已有纹理）→ `BuildSceneSet1` → `ExecuteFrameRenderGraph`（Bake/Setup/Enqueue） |
| RDG | `Absolute` 尺寸已支持；`kRDGDirShadowAtlas` 名已预留 |

**卡点：** Set1 在图 SetupAttachments **之前**采样阴影纹理 → 必须重排相位，不能只改 Pass 声明。

## 3) 方案

### 3.1 目标结构

```text
CollectShadowRequests / BuildShadowDrawCommands
  → Acquire* 只填 metadata（类型/分辨率/unit/layers；Texture=null）
Configure ShadowGraphPass + Bake（声明 Absolute depth）
SetupAttachments（图创建/复用物理纹理）
BindGraphShadowTextures → 填 Handle.Texture / ctx handles
BuildSceneSet0 / Set1
EnqueueRenderPasses（Shadow → Sky → …）
Publish SceneColor/Depth
```

### 3.2 图资源契约

| 光类型 | 逻辑名 | Dimension | Layers | SizeClass |
|--------|--------|-----------|--------|-----------|
| Directional CSM | `DirShadowAtlas`（`kRDGDirShadowAtlas`） | Texture2DArray | `MAX_CASCADES` | Absolute（请求分辨率） |
| Spot | 既有 `ShadowDepth.<i>` | Texture2D | 1 | Absolute |
| Point | 既有 `ShadowDepth.<i>`（同灯多 face 共享名） | TextureCube | 6 | Absolute |

- 多 cascade / 多 face 的多个 `ShadowGraphPass` **声明同一逻辑名** → 同一物理槽。
- Usage：`RenderTarget | ShaderResource`（采样 Set1）。
- Format：与今一致 `DEPTH32`（`MakeDepthTextureDesc`）。

### 3.3 `ShadowResourceHandle`

- `IsValid()`：**不**再要求 `Texture != nullptr`（仅 metadata）。
- 渲染 / Set1：显式要求 `Texture` 非空（SetupAttachments 之后）。

### 3.4 删除列表（S03）

- `EnsureDirectionalResource` / `EnsureSpotResource` / `EnsurePointResource`
- `m_DirectionalShadowArray` 及 spot/point 结构体中的 `RHITextureRef Texture`
- 任何 `RegisterExternal` 阴影残留（应已无）

**保留（可选薄壳）：** TextureUnit 分配、BeginFrame/EndFrame 钩子；若无状态可整类删除，由 `ForwardRenderer` 内联 unit 常量。

### 3.5 Bake 失效

阴影 pass 数变化已重建图。另：分辨率 / cascade 层数变化时须 `m_IsBaked = false`（与 `SetBackbufferDimensions` 同类），以便 Absolute dims 重解析。

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A 图拥有 + Execute 重排 | 与 F07 一致 | 动 Execute 顺序 | **选用** |
| B Manager 创建后 RegisterExternal | 少改顺序 | 违背 F07；双轨永存 | 拒绝 |
| C 延迟 Set1 到 Enqueue 内 | 少拆方法 | 绑定藏在 Pass 里难测 | 次选 |

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Set1 仍用未绑定 Texture | 无阴影 / 崩溃 | Bind 在 Set1 前；断言/早退 |
| Depth MakeCreateDesc 丢 ShaderResource | 采样失败 | 深度也保留 usage 中的 SRV 位；声明时 AddUsage SRV |
| 多 Pass 同名 dims 不一致 | Bake 错尺寸 | Configure 后统一 Absolute；Directional 共用常量名 |
| 每帧 rebake | 成本 | 仅 dims/拓扑变才失效 |

## 6) 验收标准

- [x] Directional / Spot / Point 阴影 RT 仅由图 `SetupAttachments` 创建
- [x] `ShadowResourceManager` 无 `RHICreateTexture*`
- [ ] Editor 黄金场景阴影正常（待用户目视）
- [x] `test render-graph` + `test smoke` PASS
- [x] TD-020 → Done；Registry F08 Done

## 7) Status note

（无）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-02 | 初稿 In Progress；由 TD-020 / F07 尾升级为独立 Feature |
| 2026-08-02 | S01–S03 落地：Execute 重排；图拥有 Dir/Spot/Point；Manager 仅 metadata |
