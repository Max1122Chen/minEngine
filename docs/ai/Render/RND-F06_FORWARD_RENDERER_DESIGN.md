# RND-F06 — ForwardRenderer（Renderer / RenderGraph 职责分离）

## Meta

| Field | Value |
|-------|--------|
| **Feature ID** | `RND-F06` |
| **Type** | Refactor + Architecture |
| **Status** | **In Progress**（S00 Design Done → 实现中） |
| **Owner** | (maintainer) |
| **Last updated** | 2026-07-24 |
| **Branch** | `render` |
| **Depends on** | `RND-F01` **S0–S04 Done**（Manual 图已接主路径）；`RND-F04` **Done** |
| **Blocks** | `RND-F01` **S05+**（RDG 卫生 → Bake → 功能补全）；减轻「在 Pipeline 上帝对象上继续堆图机制」的风险 |
| **Related** | [RND-F01](./RND-F01_RENDER_GRAPH_DESIGN.md) · [RND-F03](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) · [RND-F04](./RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) · Granite `RenderPassSceneRenderer` / UE `FDeferredShadingSceneRenderer` + `FRDGBuilder` |

## TL;DR

**问题：** F01 Manual RenderGraph 已把主帧挂进图，但 **`RenderPipeline` 仍同时做两件事**——（1）渲染应用/策略（CSM、队列、UBO、选 Pass）；（2）图编排宿主（构图、External 注册、手写执行序）。这使「下一步该完善 Graph 还是清理 Pipeline」长期拧巴；图层 `FrameRenderGraphContext` 仍依赖 `RenderPipeline*`，心智模型不闭合。

**方案：** 引入 **`ForwardRenderer`** 承接今天 Pipeline 的**全部应用侧职责**；**删除 `RenderPipeline`**（终态单路径）。`RenderGraph` 只保留通用编排机制。资源所有权与「谁调 RHI 创建」写进契约表。

**不在本 Feature：** RDG 目录卫生、Bake、transient、完整 PassParameters 体系——归 **F01 在 F06 Done 之后** 按既定顺序继续（见 §1.3）。

---

## Scope

### In

| 类别 | 内容 |
|------|------|
| **心智模型** | Renderer = 渲染产品/策略 + 构图；RenderGraph = 通用编排运行时（机制通用，内容由 Renderer 装配） |
| **类型** | 新增 `ForwardRenderer`；`RenderSystem` 持有并调用它 |
| **迁移** | CSM / shadow commands / 队列 / UBO / BindingSets / Pass 实例 / 帧图构建与执行 从 `RenderPipeline` 迁出 |
| **删除** | 类 `RenderPipeline` 及双路径别名；`FrameRenderGraphContext` / Pass 侧不再持有 `RenderPipeline*` |
| **契约** | RT / Shadow / UBO / 逻辑资源名 的创建者与持有者表（§3.3） |
| **验证** | `verify.ps1` + 黄金场景目视（与 F01 S03/S04 同等） |

### Out

| 项 | 归属 |
|----|------|
| RDG 空壳文件合并、名实不符重命名、删未用 `RDGBuffer` 占位等 | **F01 S05（卫生）** |
| `Bake()`、自动拓扑、非法图失败 | **F01 S06** |
| Transient 池 / 图内 `CreateTexture` | **F01 S07（可选）** |
| Bake 之后「Renderer 调图 API 全新形态」整理 | **F01 S08**（F06 只保证职责落在 Renderer，不要求构图 API 终态） |
| Vulkan | **F05** |
| 换成 Deferred GBuffer Renderer | 另开 Feature；本 Feature 只迁现有 Forward 路径 |

---

## Reader quick start

1. **§1.3** — 与 F01 的接力顺序（必读，防 feat 反复）
2. **§3** — 目标边界与所有权表
3. **§6** — 切片与删除清单
4. 代码入口（现状）：`RenderPipeline/*`、`RenderSystem.*`、`RenderGraph/*`

---

## 1) 背景与目标

### 1.1 前因（为何现在做，而不是继续 F01-S05）

```text
RND-F02/F04  现代 RHI + Packet/Binding          → Done
RND-F01 S0–S04  Manual 图 + 主帧/Shadow 迁入    → Done（2026-06-12）
               ↓
         暴露结构问题：图的纪律有了，
         「谁是 Renderer、谁是 Graph」没钉死
               ↓
RND-F06      ForwardRenderer + 删除 Pipeline     → 本文（先做）
RND-F01 S05+ RDG 卫生 → Bake → 功能补全 → 调图形态 → 再开
```

F01 设计曾默认下一步是 **S05 Bake**。复盘后认为：在 `RenderPipeline` 仍是上帝对象时做 Bake，会把**策略与机制**继续焊在一起，后续拆成本更高。故 **口径调整：F01 S05+ 显式依赖 F06 Done**（见 F01 设计案同步修订）。

### 1.2 北极星

> **`ForwardRenderer` 决定「这一帧怎么画」并装配 `RenderGraph`；`RenderGraph` 只负责「按契约跑图」。**  
> 机制谁来用都一样；本帧有哪些 Pass、叫什么资源名，由 Renderer 决定。

对照：

| 引擎 | 策略 / 构图 | 通用图 |
|------|-------------|--------|
| UE | `FSceneRenderer` / `FDeferredShadingSceneRenderer` | `FRDGBuilder` |
| Granite | scene / post `setup_*` + `RenderPassInterface` | `RenderGraph` |
| minEngine 目标 | `ForwardRenderer` | `RenderGraph` |

### 1.3 接力顺序（维护者拍板）

| 顺序 | Feature / Slice | 交付 |
|------|-----------------|------|
| 1 | **F06**（本文） | `ForwardRenderer` 取代 `RenderPipeline`；职责与所有权表落地 |
| 2 | **F01 S05** | 整理现有 RDG 实现（删空壳、改名实不符、收敛文件；**不**扩功能） |
| 3 | **F01 S06+** | Bake 等，把 RDG **机制**做完 |
| 4 | **F01 S08** | 在机制可用后，把 Renderer 侧调图整理成稳定形态 |

本 Feature **不做** 第 2–4 步的实现，但 Design / ACTIVE_WORK 必须写清，避免再次「不知道先做哪个」。

### 1.4 成功长什么样

- 代码库中 **无** `class RenderPipeline`；`RenderSystem` 只认识 `ForwardRenderer`。
- CSM、队列、UBO 更新、shadow command 构建 仅出现在 Renderer（或其明确子系统，如已有 `ShadowResourceManager`），**不**出现在 `RenderGraph` API。
- `RenderGraph` 公共 API 不出现 `ForwardRenderer` / CSM / Light 类型。
- 行为与 F01 S04 后一致（同场景目视 + smoke / render-graph 测试）。

---

## 2) 现状

### 2.1 `RenderPipeline` 混杂职责（证据）

`RenderPipeline` 同时持有：

- **策略：** `CalculateCascadeSplits`、`BuildDirectionalShadowDrawCommands`、`BuildRenderQueue`、`UpdatePerFrameUBO` / `UpdateLightUBO`
- **产品 Pass：** `ShadowPass` / `SkyBoxPass` / `BasePass` / …
- **图宿主：** `m_FrameRenderGraph`、`BuildFrameRenderGraph`、`ExecuteFrameRenderGraph`、`RegisterExternal`、手写 `PassExecutionOrder`
- **部分资源：** `m_PostBufferTexture`、整组 UBO；SceneColor/Depth 仍在 `SceneRenderTarget`（Viewport），Shadow 在 `ShadowResourceManager`

调用点：`RenderSystem` 持有 `m_RenderPipeline`，`Execute(drawDesc)` 入口。

### 2.2 图侧对 Pipeline 的泄漏

- `FrameRenderGraphContext::Pipeline` → `RenderPipeline*`
- `RenderPassBase::pipeline`、`SceneMeshDrawUtils` 取 Binding/UBO 均依赖 Pipeline

说明 Manual 迁移只换了「执行壳」，**依赖箭头仍指向旧上帝对象**。

### 2.3 已知限制（F06 承认、不在此修）

- 图内纹理仍多为 External；图不分配 RT（F01 后续）。
- `PassParameters` 几乎空壳；Pass 仍成员注入（F01 / S08）。
- RDG 目录过碎、空 TU、名实不符（F01 S05）。

---

## 3) 方案

### 3.1 目标模块边界

```text
RenderSystem
  └── SceneRenderer*                 // 薄抽象：System 不关心 Forward/Deferred
        └── ForwardRenderer          // 今天唯一实现（应用侧）
              ├── ShadowResourceManager / CSM / queues / UBO / SceneBindings
              ├── Pass 实现类（IRenderPass：Sky/Base/…）
              └── RenderGraph + FrameResources   // 持有并调用；不把策略 API 下沉进 Graph
                    └── Setup → Prepare → Build（机制）

SceneRenderTarget（Viewport）── RegisterExternal ──► FrameResources
```

**目录建议（实现时可微调，Design 约束语义）：**

- `Render/SceneRenderer.h` —— 薄基类 / 接口（仅生命周期与 `Execute` 等 System 可见 API）
- `Render/ForwardRenderer.*`（或暂留原 `RenderPipeline/` 目录改类名）—— Forward 实现
- `Render/RenderGraph/` —— 仅通用图
- 现有 `RenderPipeline/RenderPasses/`、`Shadow/` —— **随迁移改 include 路径**；允许先挪类再改目录，但 **F06 Done 前删除 `RenderPipeline` 类名**
- **不做：** 基类里预埋 GBuffer / Deferred 虚钩子；等 Deferred Feature 再扩展

### 3.2 数据流（一帧）

```text
SceneRenderer::Execute → ForwardRenderer::Execute(SceneDrawDesc)
  1. 策略 CPU：队列、CSM、shadow commands、UBO、SceneBindings
  2. 构图/绑资源：EnsurePostBuffer、RegisterExternal、配置 Shadow graph passes、SetPassExecutionOrder
     （F06 阶段可仍手写顺序；Bake 属 F01）
  3. RenderGraph::SetupAttachments + ExecuteGraph
  4. 各 IRenderPass::PreparePass / BuildRenderPass
```

### 3.3 资源所有权契约（F06 钉死）

| 资源 | 谁创建（调 RHI） | 谁持有 | 图角色（F06 后） |
|------|------------------|--------|------------------|
| SceneColor / SceneDepth | `SceneRenderTarget`（Viewport） | Viewport | External，Renderer 每帧注册 |
| PostBufferA | **ForwardRenderer**（自 Pipeline 迁出） | ForwardRenderer | External |
| Shadow atlas / maps | `ShadowResourceManager` | 该 Manager（Renderer 拥有 Manager） | External |
| Backbuffer / Present 目标 | Viewport / 呈现路径 | Viewport | External 或 Present Pass 直接取 |
| PerFrame / Lights / PerObject / Shadow UBOs | **ForwardRenderer** | **ForwardRenderer（有意）** | **不**进 Graph 类型系统；Pass 经 Renderer/Bindings 使用 |
| 逻辑名 `"SceneColor"` 等 | Renderer/Pass Setup 声明 | Graph 注册表只记名与 External 指针 | 机制层 |

**原则：** Graph **不**知道 CSM；Renderer **不**实现 Bake/barrier 算法。

**UBO 仍挂在 Renderer ≠ 退回旧 Pipeline：**  
旧问题是 **同一对象兼策略与图运行时**。帧级 UBO 属于渲染**应用/策略**状态（对齐 UE `FViewInfo` UB、Granite 侧 scene/pass 分配或子系统 buffer），**应留在 Renderer**；禁止把引擎业务 UBO 布局塞进 `RenderGraph` 公共 API。Graph 至多将来跟踪 buffer 句柄，不拥有 `LightsData` 语义。

### 3.4 API 契约（草案）

```text
class SceneRenderer {  // 薄：供 RenderSystem 持有；无 Deferred 预留钩子
  virtual ~SceneRenderer() = default;
  virtual void Initialize() = 0;
  virtual void Shutdown() = 0;
  virtual void Execute(const SceneDrawDesc& desc) = 0;
  virtual void SetPresentPassEnabled(bool) = 0;
  virtual void LoadEngineRenderingAssets(...) = 0;
};

class ForwardRenderer : public SceneRenderer {
  // 上列虚函数 + 供 Pass/utils：GetSceneBindings / GetPipelineLayouts / GetPerObjectUBO 等
};
```

- `RenderSystem` 持有 `SceneRenderer`（指针/`unique_ptr`），构造 `ForwardRenderer`；**不** include 图内部细节。
- `RenderGraph` 公共头：**禁止** include `ForwardRenderer.h` / `SceneRenderer.h`。
- Pass / `FrameRenderGraphContext` 需要 Bindings/UBO 时持有 **`ForwardRenderer*`**（或更窄的应用侧接口），**不是** `SceneRenderer*` 上堆业务 API。

### 3.5 删除清单（true refactor）

| 删除项 | 说明 |
|--------|------|
| `class RenderPipeline` | 含 `.h/.cpp`；禁止 `using RenderPipeline = ForwardRenderer` 长期残留 |
| `RenderSystem::m_RenderPipeline` | → `std::unique_ptr<SceneRenderer>`（实现为 `ForwardRenderer`）或等价 |
| 一切 `RenderPipeline*` 成员 | System 侧 → `SceneRenderer*`；Pass/utils → `ForwardRenderer*` 或更窄接口 |
| 双路径「Pipeline 转发到 Renderer」 | F06 Done 时必须为零 |

**保留（可改宿主）：** `ShadowResourceManager`、`ShadowPass` 工具方法、`SceneMeshDrawUtils`、各 `IRenderPass` 实现、`RenderGraph` 本体。

### 3.6 迁移顺序

1. 新增 `ForwardRenderer`，**搬移** Pipeline 实现（优先 move，避免行为分叉）。
2. `RenderSystem` 切换调用；全库编译通过。
3. 改 FrameContext / PassBase / utils 指针类型。
4. 删除 `RenderPipeline` 文件与残留符号。
5. 文档 / 注释中的「Pipeline 上帝对象」表述改为 Renderer（含 F01 现状段若仍写旧结构）。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 抽出 `ForwardRenderer`，删除 `RenderPipeline` | 心智闭合；单路径 | 一次改动面大 | **选用** |
| B. 保留 Pipeline 名，只内嵌 Graph | 改名成本低 | 继续污染「Pipeline」语义 | 否 |
| C. 先 F01 Bake 再拆 Renderer | 图机制先完整 | 机制焊在上帝对象上 | 否（已否决） |
| D. F06 顺带做完 RDG 卫生+Bake | 一次会话感强 | 范围爆炸、易再反复 | 否；按 §1.3 接力 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 大文件搬迁导致隐性回归（阴影/Post） | 高 | 切片：先搬迁编译绿 → 再删旧类；每切片黄金场景 |
| 改名未改依赖，残留 `RenderPipeline` 字符串 | 中 | Done 门禁：全库 grep 为零（测试/文档历史除外） |
| 目录与类名短期不一致 | 低 | 允许 S01 先类后目录；S02 收路径 |
| 与 F03 In Progress 交错 | 中 | F06 不碰 Legacy 清零尾；不扩 Vulkan |
| 文档口径再次分叉 | 中 | ACTIVE_WORK + F01 Meta/切片同步改；本文 §1.3 为顺序真源 |

---

## 6) 执行切片

| Slice | 名称 | 验收 |
|-------|------|------|
| **S00** | Design | 本文；Registry + ACTIVE_WORK + F01 口径同步 |
| **S01** | 抽出 `ForwardRenderer` | 实现迁入；可暂留 Pipeline 薄壳转发 **仅本切片内**；编译通过 |
| **S02** | 切换 `RenderSystem` + 删 Pipeline | 无 `class RenderPipeline`；Context/Pass 指针已换；`verify` + 黄金场景 |
| **S03** | 契约收尾 | 所有权与注释/目录与 §3 一致；grep 清洁；PROGRESS 记一笔 |

---

## 7) 验收标准（Feature Done）

- [ ] `SceneRenderer` 薄基类存在；`ForwardRenderer` 为唯一实现；`RenderSystem` 经基类调用
- [ ] 全库无 `class RenderPipeline`（及 `.h` 被编译进目标）
- [ ] CSM / 队列 / UBO / 构图调用均在 ForwardRenderer 侧；`RenderGraph` 无策略 API；UBO 留在 Renderer 为有意设计（§3.3）
- [ ] `FrameRenderGraphContext`（或后继）不再依赖 `RenderPipeline`
- [ ] `.\scripts\verify.ps1` PASS；黄金场景目视（dir/point/spot + 阴影）与迁前一致
- [ ] ACTIVE_WORK 主线切回 **F01 S05（RDG 卫生）**

---

## 8) 已拍板

| ID | 决定 |
|----|------|
| D1 | 实现类 **`ForwardRenderer`**；对外抽象 **`SceneRenderer`**（薄，无 Deferred 预留钩子） |
| D2 | **删除** `RenderPipeline`，不长期别名 |
| D3 | Graph **机制** vs Renderer **策略+构图**；内容由 Renderer 装配 |
| D4 | 资源所有权以 §3.3 为准；**帧 UBO 留在 ForwardRenderer**（≠ 旧 Pipeline 问题）；F06 不引入图内 CreateTexture |
| D5 | 与 F01 接力顺序以 **§1.3** 为准；F01 S05+ 依赖 F06 Done |
| D6 | RDG 卫生 / Bake / 调图终态 **Out**；写入 F01 切片，不塞进 F06 |
| D7 | `RenderSystem` 只依赖 `SceneRenderer`；Pass 取 UBO/Bindings 依赖 `ForwardRenderer`（或更窄接口） |

---

## 9) Status note

（仅 Blocked / Deferred / Cancelled 时填）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-07-24 | 初稿：心智模型、所有权、删除清单、与 F01 接力；S00–S03 |
| 2026-07-24 | 补 D1/D4/D7：`SceneRenderer` 薄基类；UBO 留 Renderer 有意澄清；System vs Pass 依赖分层 |
