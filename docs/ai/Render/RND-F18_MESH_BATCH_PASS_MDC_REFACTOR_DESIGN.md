# RND-F18 — MeshBatch + Pass 生成 MDC（Proxy 虚提交重构）

## Meta
- **ID:** RND-F18
- **Type:** Refactor（升级渲染入队契约；可伴随 Renderer / Pass 流程重组）
- **Status:** Done（S01–S02；S03 stub / `verify.ps1` Deferred）
- **Owner:** project maintainer
- **Last updated:** 2026-09-21（Editor 手验通过；收口 S01–S02）
- **Worktree / branch:** `D:\Dev\GitRepo\minEngine-render` · `feat/render`
- **Related:**
  - 无单独 Implementation 文件；切片记录在本文 §8
  - [RND-F06](./RND-F06_FORWARD_RENDERER_DESIGN.md) · [RND-F13](./RND-F13_MANUAL_RENDERER_DESIGN.md) · [RND-F16](./RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)
  - [MAT-F02](./Material/MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md)（`MaterialRenderProxy*` 寿命）
  - UE（只读）：`FPrimitiveSceneProxy` · `FMeshBatch` · `FMeshPassProcessor`（**无** `FMeshSceneProxy`）
  - 发现场景：minCraft 程序化网格（体素 mesher **不进** Core）

## TL;DR

**问题：** `ForwardRenderer::BuildRenderQueue`（及 Manual 等价路径）用 `dynamic_cast` 区分 Static / Skeletal / Sprite，在 Renderer 内联拼 `MeshDrawCommand`。扩展新网格类型必须改 Renderer；游戏 DLL 自定义 Proxy 也无法被引擎 RTTI 认出。

**方案：** 对齐 UE 边界（按本引擎裁剪）——`PrimitiveComponent::SyncSceneProxy` 虚同步镜像；`PrimitiveSceneProxy` 虚函数提交 **`MeshBatch`（Pass-agnostic）**；**每个 Pass 自己的 `MeshPassProcessor` 子类**决定是否接纳 Batch 并生成 **`MeshDrawCommand`**。基类 Processor **不**写死 Pass 种类开关。不引入 `MeshSceneProxy`。心态：**体系升级 + 真重构**。

**状态：** S01–S02 **Done**（2026-09-21 Editor 手验通过）。S03 扩展 stub 与默认 `verify.ps1` **Deferred**。

## Scope
- **In:**
  - 数据模型：`MeshBatch`、`MeshBatchCollector`；厘清 Batch vs MDC vs Packet
  - `PrimitiveSceneProxy::GetMeshBatches`（虚）；Static / Skeletal / Sprite 迁入虚提交
  - `MeshPassProcessor` 抽象基类：`AddMeshBatch` 纯虚；Opaque / Translucent / Shadow **各一个子类**（策略在 Pass 侧）
  - 统筹者只做：收集 Batch，再依次交给已注册的 Processor；**不**内含 `MeshPassKind` 开关
  - `ForwardRenderer` / `ManualRenderer` 入队流程改为 Collect → 分发给各 Pass Processor；**删除** mesh 叶子类型 `dynamic_cast` 入队分支
  - `PrimitiveComponent::SyncSceneProxy`；`RenderScene::UpdatePrimitive` **删除** Static/Skeletal/Sprite 的 `dynamic_cast` 同步
  - 必要范围内重组 Shadow / Opaque / Translucent 的队列来源（可接受行为等价的 Pass 接线调整）
  - 扩展证明：Tests stub 自定义 Proxy（或文档 + grep 门禁）
  - 文档：如何新增一种 mesh Proxy；与 `MaterialRenderProxy` 寿命约定交叉引用
- **Out（本期）：**
  - `MeshSceneProxy` 中间基类（已否决；学 UE：叶子直挂 `PrimitiveSceneProxy`）
  - Voxel / 区块 mesher / 方块图集 / 特殊光照 / 流体；改 minCraft 仓
  - 完整 UE Static-cache vs Dynamic per-view 双路径、PSO precache、`EMeshPass` 大表
  - 官方 `DynamicMeshComponent`（**后续**；见 §3.8）
  - 重写 RDG / RHI；改 `MeshDrawPacket` 语义（除非 Process 接线强制最小改动）
  - 灯 / Sky / Widget 的 `Update*` 类型分支（本期只清 **Primitive** 同步）

## Reader quick start
1. §3.1–3.3 数据模型与帧工作流（必读）
2. §3.4 API 契约 · §3.5 删除列表
3. §6 验收 · §8 建议切片
4. 代码入口（改前）：`PrimitiveSceneProxy.h` · `ForwardRenderer::BuildRenderQueue` · `SceneRenderContext.h` · `MeshDrawCommand.h` · `BasePass` / `ShadowPass` / `TranslucentPass`

---

## Pre-flight（摘要）

| 项 | 判断 |
|----|------|
| Prerequisites | Proxy / MDC / Forward+Manual / MAT-F02 Proxy 绑定 → **sound** |
| Tech-debt risk | **medium** — 入队是渲染主轴；需行为零回归 + Manual 同改 |
| WIP | Primary 空闲；`feat/render` 落后 `master` → 实现前先追上 |
| Philosophy | **Capabilities**：通用 mesh 提交；体素 opinion 留游戏仓 |
| 性质 | **True refactor**（目标契约 + 删除类型开关表），非 band-aid |
| Recommendation | **Go** — Design 确认后按切片实现 |

---

## 1) 背景与目标

### 1.1 Pain

1. 「有哪些可画 mesh」锁在 Renderer 的类型表里。  
2. 与材质族消灭 Draw 路径 `dynamic_cast` 的方向不一致。  
3. 外仓程序化网格（含日后自定义 Proxy）缺少稳定扩展点。

### 1.2 成功标准（产品语言）

> 引擎用 `PrimitiveSceneProxy` 虚函数收集 `MeshBatch`；Pass 侧生成 `MeshDrawCommand`。Renderer **不再**对每种 mesh Proxy 做 `dynamic_cast` 入队。任意 Maximum 游戏仓可做程序化网格（含自定义 Proxy），无需把体素世界塞进 Core，也无需每次改 Renderer 类型开关表。

### 1.3 对标 UE（裁剪）

| UE | 本引擎本期 |
|----|------------|
| `FPrimitiveSceneProxy` 虚提交 | `PrimitiveSceneProxy::GetMeshBatches` |
| `FMeshBatch` | `MeshBatch`（字段按现状裁剪） |
| `FMeshPassProcessor::AddMeshBatch` 纯虚；`FBasePassMeshProcessor` / `FShadowDepthPassMeshProcessor` 等子类自带策略 | 同样：抽象 `AddMeshBatch`，Pass 子类决定接纳与填 MDC；统筹者不写 Pass 开关 |
| Static cache / Dynamic 双路径 | **不做**（单路径：每帧 Collect + Process） |
| `FMeshSceneProxy` | **不存在于 UE**；本引擎也不引入 |

---

## 2) 现状

### 2.1 调用链（改前）

```text
RenderScene::m_PrimitiveSceneProxies
        │
        ▼
ForwardRenderer::BuildRenderQueue
  dynamic_cast Static / Skeletal / Sprite
  → 手填 MeshDrawCommand
  → OpaqueQueue / TranslucentQueue
        │
        ├─► ShadowPass.m_OpaqueQueue  ← 拷贝 OpaqueQueue
        ├─► BasePass.m_DrawCommands
        └─► TranslucentPass.m_DrawCommands
              │
              ▼
         MeshDrawPacket / RHI draw
```

### 2.2 类型与字段

| 类型 | 角色 |
|------|------|
| `PrimitiveSceneProxy` | 基类已持 VB / IB / VIL / `MaterialRenderProxy*` / Transform / CastShadow |
| `StaticMeshSceneProxy` | 几乎空壳 |
| `SkeletalMeshSceneProxy` | `m_BonePalette` |
| `SpriteSceneProxy` | Texture、`m_ModelMatrix`、`m_bNeedsTranslucentPass` 等 |
| 灯 / Sky / Widget | **分表**；不进 Primitive 入队（保持） |

### 2.3 已知限制

- `MeshDrawCommand` 注释写「Pass-agnostic、由 BuildRenderQueue 构建」——把 Pass 策略与类型知识揉在一起。  
- `RenderScene::UpdatePrimitive` 用 Component/Proxy 叶子 `dynamic_cast` 同步缓冲（文件内 TODO 称 hack）。**纳入本期**：改为 `PrimitiveComponent::SyncSceneProxy`。

---

## 3) 方案

### 3.0 设计原则

1. **Proxy 提交描述，不拥有 Pass。** Proxy 不「发送」MDC，不选择 Base vs Shadow。  
2. **Batch 是中间层；MDC 是 Pass 执行项；Packet 是 GPU 提交态。** 三层职责不得回混。  
3. **Pass 策略在 Pass 自己的 Processor 里。** 统筹者只分发 Batch，不写 `switch (PassKind)`。  
4. **场景不知道叶子 Component 类型。** 同步走组件虚函数；组件可以 `static_cast` 自己创建的 Proxy。  
5. **扩展 = 新 Proxy 实现虚提交**（必要时新 Pass Processor）；Renderer 类型开关表删除。  
6. **真重构：** 旧 `dynamic_cast` 入队与 Primitive 同步路径在落地切片中删除，不长期双轨。  
7. **行为优先零回归：** 画面 / 阴影 / 半透明分队语义与改前一致，再谈扩展 stub。  
8. **机制非意见：** 无 Voxel* 类型名进入 Core。

### 3.1 数据模型

```text
┌─────────────────────┐
│ PrimitiveSceneProxy │  渲染镜像（变换、阴影、可画 mesh 缓冲指针…）
└──────────┬──────────┘
           │ GetMeshBatches (virtual)
           ▼
┌─────────────────────┐
│     MeshBatch       │  Pass-agnostic 几何 + 材质描述（可多条 / Proxy）
└──────────┬──────────┘
           │ 各 Pass 的 MeshPassProcessor::AddMeshBatch
           ▼
┌─────────────────────┐
│  MeshDrawCommand    │  某 Pass 可排序/可过滤的逻辑绘制项
└──────────┬──────────┘
           │ Pass Prepare / Execute（现有）
           ▼
┌─────────────────────┐
│  MeshDrawPacket     │  PSO + Binding + IB/VB 范围（RHI 提交）
└─────────────────────┘
```

#### 3.1.1 `MeshBatch`（新建）

Pass-agnostic。建议字段（实现时可微调命名，语义保留）：

| 字段 | 含义 |
|------|------|
| `RHIBuffer* VertexBuffer` | 非拥有 |
| `RHIVertexInputLayout* VertexInputLayout` | 非拥有 |
| `RHIBuffer* IndexBuffer` | 非拥有；可空（非索引绘制若未来需要） |
| `MaterialRenderProxy* MaterialProxy` | 非拥有；寿命见 MAT-F02 |
| `Matrix4 LocalToWorld` | 世界矩阵（Sprite 用已含 Size 的矩阵） |
| `Math::Geometry::AABB Bounds` | 供阴影/剔除等；可来自 Component |
| `bool CastShadow` | 是否参与 ShadowDepth |
| `const Matrix4* BonePalette` / `BoneCount` / `bSkinned` | 骨骼可选 |
| `PrimitiveSceneProxy* Proxy` | 可选回指（调试 / 将来 Pass 查询） |
| `uint16_t BatchIdInPrimitive` | 稳定排序；单 Section 时为 0 |

**不放入 Batch：** Pass 名、PSO、BindingSet、具体 shader 变体选择。

**有效性：** Processor 侧拒绝 `!VB \|\| !VIL \|\| !MaterialProxy` 的 Batch（与今日 `enqueueCommand` 一致）。Sprite 无 Texture 时 **Proxy 不提交 Batch**（把门槛留在 Collect，避免 Renderer 认 Sprite）。

#### 3.1.2 `MeshBatchCollector`（新建）

帧临时收集器（可挂在 `SceneRenderContext` 或 Renderer 栈上）：

- `AddMesh(const MeshBatch&)`  
- `const std::vector<MeshBatch>& GetBatches() const`  
- `Clear()`

不拥有 GPU 资源；Batch 内指针必须在 Draw 完成前保持有效（与今日 MDC 相同：缓冲属 Mesh/资产，材质 Proxy 属材质对象）。

#### 3.1.3 `MeshDrawCommand`（保留并纠偏语义）

- **保留**现有字段与 Pass 消费方式，降低回归面。  
- **纠偏注释：** MDC 由 **PassProcessor** 从 `MeshBatch` 生成，供 Shadow / Opaque / Translucent 等 Pass 使用；**不是** Proxy 的公共玩法 API。  
- SortKey 等可继续由 Processor 或 Pass 填写（本期可不深化排序）。

#### 3.1.4 `MeshDrawPacket`（基本不动）

仍为「单次 GPU draw 的完整状态」（RND-F04）。Pass 内 MDC→Packet 逻辑保持；本 Feature 主改 **MDC 从何而来**。

#### 3.1.5 `SceneRenderContext`（演进）

建议：

| 成员 | 角色 |
|------|------|
| `std::vector<MeshBatch> MeshBatches` | 本帧 Collect 结果（可仅调试保留；或 Process 后可 clear） |
| `OpaqueQueue` / `TranslucentQueue` | **仍为** `MeshDrawCommand` 列表，由 Processor 填充 |
| Shadow 输入 | **推荐：** Processor 直接产出 `ShadowQueue`（`CastShadow` 过滤），或 ShadowPass 继续消费「可投影」MDC 列表——**删除「Shadow = 盲拷贝 OpaqueQueue」的隐含假设**，改为显式规则（见 §3.3） |

灯 / Sky / Widget / ScreenUI 队列与现网一致，不经 `MeshBatch`。

### 3.2 接口契约

#### 3.2.0 `PrimitiveComponent::SyncSceneProxy`

`RenderScene::UpdatePrimitive` 只负责生命周期（无 Proxy 则 `CreateSceneProxy` 并登记），然后调用：

```cpp
class PrimitiveComponent
{
public:
    /** Push game-thread state into the render mirror. Scene does not downcast. */
    virtual void SyncSceneProxy(PrimitiveSceneProxy& proxy);
};
```

基类实现写公共字段：`m_Transform = GetWorldTransform()`、`m_CastShadow = CastShadow()`。

| 组件 | 覆盖 |
|------|------|
| `StaticMeshComponent` | 先调基类，再写 VB / VIL / IB / `MaterialRenderProxy*`（`static_cast<StaticMeshSceneProxy&>`，因为本组件是该 Proxy 的工厂） |
| `SkeletalMeshComponent` | 现有 `SyncSceneProxy(SkeletalMeshSceneProxy&)` 收进虚函数 |
| `SpriteComponent` | 现有 `UpdateSceneProxy` 收进虚函数 |

**允许**组件对自己创建的 Proxy 做 `static_cast`。**禁止** `RenderScene` 再按叶子类型 `dynamic_cast`。

#### 3.2.1 `PrimitiveSceneProxy`

```cpp
class PrimitiveSceneProxy
{
public:
    virtual ~PrimitiveSceneProxy() = default;

    /** Emit zero or more pass-agnostic mesh descriptions. Default: one batch from base fields if valid. */
    virtual void GetMeshBatches(MeshBatchCollector& collector) const;

    // existing fields: Transform, CastShadow, VB/IB/VIL, MaterialProxy, ...
};
```

**默认实现：** 若 `m_VertexBuffer && m_VertexInputLayout && m_MaterialProxy`，则 `AddMesh` 一条：矩阵 = `m_Transform.ToMatrix()`，骨骼空，`CastShadow = m_CastShadow`，Bounds 若基类无则由 Processor/调用方从 `m_PrimitiveComponent` 补（保持今日行为）。

| 叶子 | Collect 行为 |
|------|----------------|
| `StaticMeshSceneProxy` | 可用默认 |
| `SkeletalMeshSceneProxy` | override：填 `BonePalette` / `bSkinned` |
| `SpriteSceneProxy` | override：无 `m_Texture` 则不提交；矩阵用 `m_ModelMatrix`；`CastShadow = false`；半透明路由见下 |

**不**为灯/Sky/Widget 实现本虚函数（它们不在 `m_PrimitiveSceneProxies`）。

#### 3.2.2 `MeshPassProcessor`（抽象；策略在子类）

UE（本机源码）：`FMeshPassProcessor::AddMeshBatch` 是纯虚。`FBasePassMeshProcessor`、`FShadowDepthPassMeshProcessor` 各自实现接纳规则（例如阴影子类先看 `MeshBatch.CastShadow`，再 `TryAddMeshBatch`）。场景代码持有**具体 Processor 实例**并调用它；基类里没有「枚举所有 Pass 再 switch」的政策中心。`EMeshPass::Type` 只是 Pass 标识，不是把绘制策略写进一个函数。

本引擎按同样方向裁剪（不做 PSO precache / 每 Pass 巨量 shader 选择）：

```cpp
class MeshPassProcessor
{
public:
    virtual ~MeshPassProcessor() = default;

    /** Pass-owned policy: accept or ignore this batch; append MDCs this pass wants. */
    virtual void AddMeshBatch(const MeshBatch& batch) = 0;
};

class OpaqueMeshPassProcessor : public MeshPassProcessor { /* ... */ };
class TranslucentMeshPassProcessor : public MeshPassProcessor { /* ... */ };
class ShadowDepthMeshPassProcessor : public MeshPassProcessor { /* ... */ };
```

**统筹者**（自由函数或 Renderer 私有步骤，不是政策开关）：

```cpp
void DispatchMeshBatches(
    const std::vector<MeshBatch>& batches,
    std::span<MeshPassProcessor*> processors);
// for each processor: for each batch: processor->AddMeshBatch(batch);
```

公共字段拷贝（VB、矩阵、骨骼、材质指针）放在 **非虚辅助** `MeshDrawCommand MakeMeshDrawCommand(const MeshBatch&)`，避免三个子类复制粘贴。子类只写「接不接、队列往哪放、有没有 Pass 特例」。

**各子类的今日对齐规则**（写在子类，不写在基类 switch）：

| 子类 | 接纳 |
|------|------|
| `OpaqueMeshPassProcessor` | 几何有效；且非半透明路径（`!IsTranslucent()` 且非 `bForceTranslucentPass`） |
| `TranslucentMeshPassProcessor` | 几何有效；且 `IsTranslucent()` 或 `bForceTranslucentPass` |
| `ShadowDepthMeshPassProcessor` | 几何有效；`CastShadow`；半透明路径不进阴影（对齐今日只拿 Opaque 队列） |

Sprite 的强制半透明仍是 Batch 标志，不是 Processor 里的 `dynamic_cast<SpriteSceneProxy*>`。

新增一种 Pass = 新的 Processor 子类并注册给统筹者。**不必**改基类或加枚举分支。

#### 3.2.3 Renderer

`ForwardRenderer` / `ManualRenderer` 共享同一 Collect → Process 顺序（可抽自由函数或 `SceneMeshDrawBuilder`，避免两处复制规则）：

1. `collector.Clear()`  
2. 遍历 `renderScene->m_PrimitiveSceneProxies` → `proxy->GetMeshBatches(collector)`  
3. `DispatchMeshBatches(batches, { opaque, translucent, shadow })` — 各 Processor 自己 `AddMeshBatch`  
4. 将各 Processor 产出的 MDC 队列交给现有 Pass 成员（允许微调成员名，但 RDG Pass 图可保持）

**门禁：** `BuildRenderQueue`（或继任函数）中 **不得**出现对 `StaticMeshSceneProxy` / `SkeletalMeshSceneProxy` / `SpriteSceneProxy` 的入队 `dynamic_cast`。

### 3.3 帧工作流（目标）

```text
[Game]  UpdatePrimitive
          create proxy if needed
          component->SyncSceneProxy(*proxy)     // virtual; scene has no leaf casts
            │
            ▼
[Collect]  for proxy in PrimitiveSceneProxies:
              proxy->GetMeshBatches(collector)
            │
            ▼
[Dispatch] for each registered MeshPassProcessor:
              processor->AddMeshBatch(batch)    // policy lives in the subclass
            │
            ├─ OpaqueMeshPassProcessor      → OpaqueQueue
            ├─ TranslucentMeshPassProcessor → TranslucentQueue
            └─ ShadowDepthMeshPassProcessor → ShadowQueue
            │
            ▼
[Pass Execute]  ShadowPass / BasePass / TranslucentPass
              MDC → MeshDrawPacket → RHI
```

**可接受的 Pass 侧重构：**

- Shadow 不再 `m_OpaqueQueue = ctx.OpaqueQueue` 盲拷贝，改为消费 `ShadowDepthMeshPassProcessor` 产出的队列。  
- `BuildRenderQueue` 可改名为 `CollectAndBuildMeshDraws`（或拆成 Collect + Dispatch）。  
- 本期要三个 Processor **子类**（策略倒置）；不要基类里的 `MeshPassKind` switch。不做 UE 级 PSO precache。

### 3.4 模块边界

| 模块 | 拥有 | 不拥有 |
|------|------|--------|
| `PrimitiveComponent` | 虚 `SyncSceneProxy`；对自己的 Proxy 类型知情 | 场景级类型表 |
| `RenderScene` | Proxy 登记/移除；调用 `SyncSceneProxy` | 叶子 Component/Proxy 分支 |
| Proxy | 镜像数据；决定 **发出哪些 Batch** | Pass、MDC、Packet、RDG |
| Collector | 本帧 Batch 列表 | GPU 资源寿命 |
| `MeshPassProcessor` 基类 | `AddMeshBatch` 契约 | 具体 Pass 接纳规则 |
| Pass Processor 子类 | 该 Pass 的过滤与 MDC 填充 | 其他 Pass 的策略；叶子 Proxy 类型 |
| 统筹者（Renderer 步骤） | 把同一批 Batch 交给已注册 Processor | `switch (PassKind)` |
| Forward/Manual Renderer | 调度 Collect/Dispatch、挂 Pass | 游戏体素语义 |
| Base/Shadow/Translucent Pass | MDC→Packet→Draw | 从 Proxy 类型拼 MDC |
| 游戏仓 | Voxel mesher、自定义 Proxy 子类 | 改引擎 Renderer 开关表 |

### 3.5 删除列表（落地后必须消失）

| 删除项 | 位置 |
|--------|------|
| `dynamic_cast<StaticMeshSceneProxy*>` 入队分支 | `ForwardRenderer::BuildRenderQueue` |
| `dynamic_cast<SkeletalMeshSceneProxy*>` 入队分支 | 同上 |
| `dynamic_cast<SpriteSceneProxy*>` 入队分支 | 同上 |
| ManualRenderer 中同类入队分支（若有） | `ManualRenderer` |
| 「MDC 由 Renderer 按 Proxy 类型手填」的设计叙述 | 注释 / 旧文档表述 |
| `UpdatePrimitive` 内 Static/Skeletal/Sprite 的 `dynamic_cast` | `RenderScene.cpp` |
| 基类 Processor 上的 `MeshPassKind` 集中 switch | 不得引入 |

**保留：** 灯 / Sky / Widget 分表及其 `Update*`（本期不扩）。组件内部 `static_cast` 到自己创建的 Proxy 类型。

### 3.6 扩展性验收设计

**Stub（推荐放 `minEngineTests`）：**

- `CollectOnlyMeshProxy : PrimitiveSceneProxy`，构造时填最小 VB/IB/VIL + 测试材质 Proxy，**只靠默认或轻量 override `GetMeshBatches`**。  
- 挂进 `RenderScene` 后，经 Collect→Process，Opaque（或约定队列）中出现对应 MDC。  
- **断言：** 实现该 stub **不需要**修改 `ForwardRenderer.cpp` 的类型分支（分支应已不存在）。

可选：`grep` / 单测扫描源文件，禁止入队路径再引入上述三种 `dynamic_cast`。

### 3.7 与 MaterialRenderProxy 寿命

沿用 MAT-F02：

- Batch / MDC 持 `MaterialRenderProxy*` **非拥有**。  
- Proxy 与材质对象同寿；换材质时由 Component 同步路径改写 SceneProxy 上的指针。  
- 本 Feature 不改变材质所有权。

### 3.8 后续（不绑本期）

| 项 | 说明 |
|----|------|
| `DynamicMeshComponent` | 官方「运行时重建 VB/IB」机制；方便外仓程序化网格 |
| 多 Section / 多 Material | 一 Proxy 多 `MeshBatch` |
| Static Batch 缓存 | 对齐 UE DrawStaticElements 缓存 MDC |
| 游戏 DLL 导出 Proxy ABI | 子类布局 / 可见性约定 |

minCraft：可继续用 StaticMesh 构造 MVP；体素 Section→网格留在游戏仓。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 仅虚函数直接填 MDC，无 Batch | 改动面更小 | Pass 策略仍易漏进 Proxy；难演进多 Pass | **否** |
| B. Proxy→Batch；Pass 子类 Processor→MDC | 策略在 Pass；对齐 UE `AddMeshBatch` | 三个小类 | **选用** |
| B2. 一个 Processor + `MeshPassKind` switch | 类型更少 | 新 Pass 必须改中心开关；与「倒置」相反 | **否**（评审否决） |
| C. 引入 `MeshSceneProxy` 中间基类 | 语义分区 | UE 无此层；本引擎 Primitive 列表已是 mesh 语义 | **否**（已拍板） |
| D. 完整 UE Mesh Drawing Pipeline | 长期最强 | 远超现状痛点与切片可控性 | **否**（后续） |
| E. 引擎 `VoxelMeshComponent` | minCraft 最快 | Opinion 进 Core | **否** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 半透明 / Sprite / 阴影分队语义漂移 | 画面回归 | 规则写在对应 Processor 子类；对照改前队列；Editor 手验 |
| Forward 与 Manual 行为分叉 | Reference 路径不可信 | 共享 Collect + `DispatchMeshBatches` |
| `feat/render` 落后 master | 合并冲突 | **实现前** rebase/merge `master` |
| 范围膨胀到 DynamicMesh | 延误主契约 | 仍 Deferred |
| Batch 堆分配每帧 | 微小开销 | 先正确；后续可 frame allocator |

---

## 6) 验收标准

- [x] `PrimitiveComponent::SyncSceneProxy`；`RenderScene::UpdatePrimitive` **无** 三种 mesh 的 `dynamic_cast`  
- [x] `PrimitiveSceneProxy::GetMeshBatches`；Static / Skeletal / Sprite 经虚分发提交 Batch  
- [x] `MeshPassProcessor::AddMeshBatch` 纯虚；Opaque / Translucent / Shadow 子类各自接纳；**无** 基类 `MeshPassKind` switch  
- [x] `ForwardRenderer`（及 Manual）入队路径 **无** 对三种 mesh Proxy 的 `dynamic_cast`  
- [x] 现有 Static / Skeletal / Sprite 画面与阴影：维护者 Editor 手验通过（2026-09-21）  
- [x] Design / Progress / Registry 收口为 Done（S01–S02）  
- **Deferred：** 默认 `verify.ps1` 本批未跑；S03 stub / grep 门禁未做

---

## 7) Status note

S03 stub 与 `verify.ps1`：**Deferred**。不挡 S01–S02 收口。重开条件：需要自动化证明「新叶子 Proxy 不改 Renderer」时再做 S03。

---

## 8) 建议切片（确认后写入 Implementation）

| Slice | Goal | Verify（示意） |
|-------|------|----------------|
| **S01** | Design + `feat/render` 追上 master | **Done** |
| **S02** | `SyncSceneProxy`；MeshBatch；三个 Processor；删 mesh cast；Forward/Manual/Shadow | **Done** — Editor 手验通过；`verify.ps1` 未跑 |
| **S03** | Stub 扩展证明 + grep 门禁 | **Deferred** |
| **S04** | 薄 `DynamicMeshComponent` | **Deferred** |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-21 | Draft：否决 MeshSceneProxy；确立 Batch→Pass→MDC；真重构心态；DynamicMesh / 体素后置 |
| 2026-09-21 | 评审：`SyncSceneProxy` 纳入本期；否决 `MeshPassKind` 中心开关，改为 Pass 子类 `AddMeshBatch` |
| 2026-09-21 | S01–S02 Done（Editor 手验）；S03 / `verify.ps1` Deferred |
