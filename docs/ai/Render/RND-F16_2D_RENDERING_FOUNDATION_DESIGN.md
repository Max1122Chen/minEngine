# RND-F16 — 2D Rendering Foundation

## Meta
- **ID:** `RND-F16`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Branch:** `feat/ui`
- **Related:**
  - [UI-F01](../Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md)（Screen / World Canvas 消费者）
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - 外部底稿：[minEngine_ui_mvp_suggestions.md](../../external/minEngine_ui_mvp_suggestions.md)
  - 代码对照：`StaticMeshComponent` / `StaticMeshSceneProxy` / `ForwardRenderer::BuildRenderQueue` / `MeshDrawCommand`
  - UE：Paper2D `UPaperSpriteComponent`；UMG `EWidgetSpace`
- **Depends on:** Modern RHI + `ForwardRenderer` / Manual RDG（可用即可）
- **Blocks:** `UI-F01` Screen-space 绘制
- **Implementation:** [RND-F16_2D_RENDERING_FOUNDATION_IMPLEMENTATION.md](./RND-F16_2D_RENDERING_FOUNDATION_IMPLEMENTATION.md)

## TL;DR

**问题：** Sprite（世界面片）与 Screen Widget（HUD）不能走同一条 Overlay 通道。

**方案：** 共享 2D 原语 + 分路径。**本 Feature 先落地 Path A：**  
`SpriteComponent` → `SpriteSceneProxy` → Opaque/Translucent Queue（同构 StaticMesh）。  
透明性 = **`Color.a` 或纹理可能含 alpha`**；MVP **不投射阴影、不做 billboard**。  
Path B（ScreenUI + Widget Proxy）仅定契约，代码后置。

## Scope

### In（本 Feature，优先 Path A）
- 共享 unit quad 几何 + Sprite unlit 绘制约定
- `SpriteComponent` / `SpriteSceneProxy` / `BuildRenderQueue` 扩展
- 透明分流规则（Color + Texture）
- Path B 数据结构与接口契约（文档）；实现可后续 Slice

### Out
- Billboard；Layout / Hit-test / Widget 行为（`UI-F01`）
- World Canvas 完整实现；完整 Paper2D / Slate；ImGui Game UI

## Reader quick start
1. §3 双路径心智  
2. **§9 Path A 详细设计（数据结构 / 接口 / 数据流）** ← 实现前必读  
3. §10 Path B 契约（后置）  
4. 代码：`PrimitiveComponent`、`StaticMeshSceneProxy`、`ForwardRenderer::BuildRenderQueue`

---

## 1) 背景与目标

见既有双路径动机：Sprite 要场景深度；Screen UI 要保序合成。成功标准：场景内可见 Sprite；Proxy 入队；`Color`/纹理 alpha 分流正确；阴影关闭。

---

## 2) 现状（代码事实）

```text
StaticMeshComponent : PrimitiveComponent
  → CreateSceneProxy() → StaticMeshSceneProxy { VB, IB, layout, Material* }
  → RenderScene::UpdatePrimitive
  → ForwardRenderer::BuildRenderQueue
       dynamic_cast<StaticMeshSceneProxy*>
       → MeshDrawCommand
       → Material::IsTranslucent() ? TranslucentQueue : OpaqueQueue
  → BasePass / TranslucencyPass
```

| 缺口 | 说明 |
|------|------|
| 无 Sprite 类型 | `BuildRenderQueue` 只认 StaticMesh |
| `MeshDrawCommand` | 已够用（VB/IB/Material/ModelMatrix/CastShadow/AABB） |
| `Texture2D` | 有 `GetChannels()`；尚无「是否可能半透」的一等 API（本 Feature 补约定） |
| Color 类型 | 工程内常用 `Vector4` 表示 RGBA（无独立 `Color` 类） |

---

## 3) 方案总览

```text
共享: unit quad + Sprite unlit material/PSO 约定
        │
   ┌────┴────┬──────────────┐
   ▼         ▼              ▼
Path A    Path B         Path C
Sprite    Screen UI      World UI（后）
↓         ↓
Opaque/   ScreenUIQueue
Translucent  ScreenUI Pass
```

帧序（Screen UI 到位后）：… → Opaque → Translucent → (Debug) → **ScreenUI** → Post → Present。

### 3.1 Path A 摘要
`SpriteComponent` : `PrimitiveComponent`；固定 Transform 朝向；`CastShadow=false`；队列由 §9.4 规则决定。

### 3.2 Path B 摘要
Widget/Canvas ∈ `SceneComponent` + Proxy；入 **ScreenUIQueue**，禁止 Opaque 距离排序。详见 §10。

### 3.3 实现顺序
```text
S0  共享 unit quad + Sprite unlit 可绘制
S1  SpriteComponent + SpriteSceneProxy + BuildRenderQueue
S2  透明分流（Color.a + texture）+ Playground/test
—— Path A MVP ——
S3+ Path B（后置）
```

---

## 4) UE 对照（学习）

| 主题 | 锚点 | minEngine |
|------|------|-----------|
| Sprite→场景 | Paper2D `CreateSceneProxy` / `GetDynamicMeshElements` | Path A 同构 |
| Screen 无 Proxy | UMG Screen → Slate 层 | **刻意分叉**：有 Proxy，进 ScreenUIQueue |
| World Widget | `FWidget3DSceneProxy` + RT | Path C 后置 |

---

## 5) 备选方案

| 选项 | 结论 |
|------|------|
| 双路径 + 共享原语 | **选用** |
| 单一 Overlay 吃 Sprite+UI | 弃用 |
| Screen UI 进 TranslucentQueue | 拒绝（破坏 UI 序） |
| MVP 每 Sprite 独立动态 VB | 拒绝；用共享 unit quad + ModelMatrix 缩放 |

---

## 6) 风险与缓解

| 风险 | 缓解 |
|------|------|
| 仅看 `Color.a` 漏掉纹理镂空 | §9.4：纹理「可能含 alpha」也进 Translucent |
| 保守半透过多伤合批 | 可接受；日后 `bForceOpaque` / Masked |
| `dynamic_cast` 堆类型 | Sprite 先对齐 StaticMesh；后可 `GatherMeshDraws` 虚函数 |
| Color 每帧改 Material | Proxy 缓存；参数 dirty 策略见 §9.5 |

---

## 7) 验收标准（Path A MVP）

- [ ] `SpriteComponent` 可挂 GO；反射可见 Texture / UV / Size / Color
- [ ] 经 `SpriteSceneProxy` 进入 Opaque **或** Translucent 队列（规则 §9.4）
- [ ] `CastShadow == false`；不进入阴影 caster 集
- [ ] 无 billboard；朝向跟 Transform
- [ ] Playground 或 test：至少 1 个不透明 + 1 个半透（Color 或纹理）Sprite 可见
- [ ] Path B 契约仍写在本文 §10（可不实现）

---

## 8) Decisions

| 项 | 决定 |
|----|------|
| 顺序 | 先 Path A（含 SpriteComponent 基础） |
| Billboard | MVP 不做 |
| 阴影 | MVP `CastShadow = false`（构造默认；可保留 API 但不进阴影） |
| 着色 | 组件 `m_Color`（`Vector4` RGBA）乘采样结果 |
| 透明分流 | **`Color.a` 不足不透明 OR 纹理可能含 alpha** → Translucent（§9.4） |
| 几何 | 共享 unit quad；世界尺寸用 Size × Transform |
| Path B Proxy | SceneComponent + Proxy 同构；代码后置 |

---

## 9) Path A 详细设计 — 数据结构 / 接口 / 数据流

### 9.1 类型与文件落点（建议）

| 类型 | 建议路径 | 职责 |
|------|----------|------|
| `SpriteComponent` | `Runtime/Function/Framework/Components/SpriteComponent.h` | 游戏线程权威数据；`CreateSceneProxy` |
| `SpriteSceneProxy` | `Runtime/Function/Render/PrimitiveSceneProxies/SpriteSceneProxy.h` | 渲染镜像（非拥有 GPU mesh） |
| `SpriteQuadMesh`（或 `SpriteGeometry`） | `Runtime/Function/Render/Sprite/` | 进程内共享 unit quad VB/IB/layout |
| `SpriteMaterialUtil` / 默认 Material | 同目录或 Material 资源 | Unlit textured + vertex/tint color |
| `ForwardRenderer::BuildRenderQueue` | 现有文件 | 增加 Sprite 分支 |

命名对齐现有：`m_` 成员、PascalCase 类型、`ME_CLASS` / `ME_PROPERTY` 反射。

### 9.2 游戏线程：`SpriteComponent`

对齐 `StaticMeshComponent` 合同：继承 `PrimitiveComponent`，实现 `GetBoundingBox` / `CreateSceneProxy`。

```text
SpriteComponent : PrimitiveComponent
{
  // --- ME_PROPERTY（示意）---
  shared_ptr<Texture2D> m_Texture;     // 必填才可画；null → 不入队
  Vector4               m_Color;       // RGBA，默认 (1,1,1,1)；乘在采样色上
  Vector2               m_Size;        // 本地/世界平面上的宽高（单位与场景一致）
  Vector4               m_UVRect;      // (u0,v0,u1,v1)，默认 (0,0,1,1)
  // 可选后置: shared_ptr<Sprite> m_SpriteAsset;

  // --- 行为 ---
  SetTexture / SetColor / SetSize / SetUVRect  → MarkRenderStateDirty()
  GetBoundingBox()  → 由 Size + 世界矩阵估 AABB（固定朝向 quad）
  CreateSceneProxy() → new SpriteSceneProxy，填入只读快照
  CastShadow: 构造时 m_CastShadow = false
}
```

**不变量：**
1. `m_Size.x/y > 0` 才生成有效 AABB/绘制；否则跳过。  
2. 朝向完全由 `SceneComponent` Transform 决定（本地 XY 平面张成 quad，**+Z 为面片法线约定**——实现时与引擎坐标一致并写进注释）。  
3. 不在组件内持有 per-instance VB（共享 unit quad）。

**与 StaticMesh 对照：**

| | StaticMeshComponent | SpriteComponent |
|--|---------------------|-----------------|
| 几何来源 | `StaticMesh` 资产 | 共享 unit quad × Size |
| 外观 | `Material` | 默认 Sprite unlit + Texture + Color（可后置 Override Material） |
| 阴影默认 | `true` | **`false`** |

### 9.3 渲染线程镜像：`SpriteSceneProxy`

```text
SpriteSceneProxy : PrimitiveSceneProxy
{
  // 非拥有：指向共享几何
  RHIBuffer*            m_VertexBuffer;
  RHIVertexInputLayout* m_VertexInputLayout;
  RHIBuffer*            m_IndexBuffer;

  Material*             m_Material;      // 默认 Sprite material 或 override
  Texture2D*            m_Texture;       // 非拥有；绑到 material/参数
  Vector4               m_Color;
  Vector4               m_UVRect;
  Vector2               m_Size;

  bool                  m_bNeedsTranslucentPass;  // Create/Update 时算好
  // m_Transform / m_CastShadow 来自基类（CastShadow 恒 false）
}
```

**创建时机：** 与 StaticMesh 相同——`PrimitiveComponent::DoEndOfFrameUpdate` → `RenderScene::UpdatePrimitive` → `CreateSceneProxy()` 或刷新已有 proxy 字段。

**ModelMatrix：**  
`WorldMatrix * Scale(m_Size.x, m_Size.y, 1)`（若 unit quad 为 [-0.5,0.5]² 或 [0,1]²，缩放与原点约定必须与 VB 一致；**实现前在代码注释钉死一种**）。推荐：**中心在原点、边长 1 的 quad**，再乘 Size。

### 9.4 透明性（Translucency）规则 — Color + Texture

用户修正后的完整规则：

```text
NeedsTranslucentPass =
     ColorAlphaIsTranslucent(m_Color.a)
  OR TextureMayHaveAlpha(m_Texture)
  OR (m_Material && m_Material->IsTranslucent())   // 若允许 override
```

| 谓词 | MVP 定义 |
|------|----------|
| `ColorAlphaIsTranslucent(a)` | `a < 1.0f - kAlphaEps`（建议 `kAlphaEps = 1.0f/255.0f`） |
| `TextureMayHaveAlpha(tex)` | `tex == null` → false；否则 **`tex->GetChannels() >= 4`**（或未来 `HasAlphaChannel()`）为 true 则视为可能半透 |
| Material override | MVP 可无 override；若有且 `IsTranslucent()`，强制 Translucent |

**语义说明：**
- **保守：** 带 alpha 通道的纹理（即使像素全不透明）也会进 TranslucentQueue——正确优先于合批。  
- **不**在 MVP 做 CPU 读回扫描「是否真有 a&lt;1 纹素」。  
- **Masked（alpha test）** 后置；镂空精灵先走半透也可接受。  
- 日后优化：`bTreatTextureAsOpaque` 或导入时烘焙 `m_bSourceHasTransparentTexels`。

**入队（BuildRenderQueue）：**

```text
if (proxy->m_bNeedsTranslucentPass)
    TranslucentQueue.push_back(cmd);
else
    OpaqueQueue.push_back(cmd);

cmd.m_CastShadow = false;  // 即使组件被改，MVP 阴影构建忽略 Sprite
```

计算时机：在 `CreateSceneProxy` / dirty 更新时写入 `m_bNeedsTranslucentPass`，避免每帧重复判纹理。

### 9.5 Material / 着色接口

**MVP 推荐：** 引擎持有一份 **默认 Sprite Unlit Material**（或等价 PSO + 最小绑定）：

- 采样 `Texture2D`  
- 输出 `sample * Color`（Color 来自 material 参数或顶点色）  
- Blend：与队列一致——Opaque 用不透明 PSO；半透用 alpha blend（可由同一 Material 的 `MaterialBlendMode` 在 dirty 时切换，或双 Material 实例）

**接口倾向（二选一，实现选默认 A）：**

| | A. 双默认 Material（推荐 MVP） | B. 单 Material 改 BlendMode |
|--|-------------------------------|-----------------------------|
| Opaque | `SpriteUnlitOpaque` | 运行时设 `Opaque` |
| Translucent | `SpriteUnlitTranslucent` | 运行时设 `Translucent` |
| 优点 | 无运行时改 blend 坑 | 少资源 |
| 缺点 | 两份 | dirty 时要同步 blend |

Proxy 上 `m_Material` 指向当前选用的那份；`MeshDrawCommand::m_Material` 照旧。

**Color / UV 如何进 GPU（MVP）：**
- **Color：** material scalar/vector 参数（改 Color → `MarkRenderStateDirty` → 更新 proxy → 更新 material 参数或 per-draw 常量）。若 material 实例按组件独享，注意别共享可变实例。  
- **UVRect：** 优先 **顶点 UV 在 VS 里用 uniform 做 remap**（`uv = lerp(uv0, uv1, attrib)`），避免每 Sprite 重建 VB。  
- 禁止 MVP 为每个 Sprite `Create` 新 VB。

### 9.6 共享几何：`SpriteQuadMesh`

```text
SpriteQuadMesh (进程单例或 ForwardRenderer 持有)
{
  RHIBuffer* VB;           // 4 顶点: pos.xyz, uv.xy  (unit square)
  RHIBuffer* IB;           // 6 indices
  RHIVertexInputLayout*;
  void EnsureInitialized(RHI&);
}
```

所有 `SpriteSceneProxy` 非拥有指针指向同一套缓冲。生命周期：Renderer/RHI 初始化时创建，关闭时释放。

### 9.7 `MeshDrawCommand` 填充

与 StaticMesh 同构，不新增 Command 类型（MVP）：

```text
MeshDrawCommand cmd;
cmd.m_VertexBuffer / Layout / IndexBuffer = 共享 quad;
cmd.m_Material     = spriteProxy->m_Material;
cmd.m_ModelMatrix  = WorldMatrix * Scale(Size.x, Size.y, 1);
cmd.m_BoundingBox  = component->GetBoundingBox();
cmd.m_CastShadow   = false;
```

若现有 `PrepareSceneMeshDrawPackets` 假定某些 material 绑定布局，Sprite material 必须满足同一绑定约定或走已有 unlit 路径——**实现 Slice 时对着 `SceneMeshDrawUtils` 验一次**。

### 9.8 接口一览（对外）

**游戏 / 编辑器 / Lua（反射）：**

| API | 说明 |
|-----|------|
| `SetTexture(shared_ptr<Texture2D>)` | dirty |
| `SetColor(Vector4)` | dirty；影响半透判定 |
| `SetSize(Vector2)` | dirty；影响 AABB / 矩阵 |
| `SetUVRect(Vector4)` | dirty |
| `Get*` | 只读 |
| `CastShadow()` | 继承；MVP 保持 false |

**渲染内部（非脚本 API）：**

| API | 说明 |
|-----|------|
| `SpriteComponent::CreateSceneProxy()` | 分配并填充 proxy |
| `SpriteComponent::GetBoundingBox()` | AABB |
| `SpriteSceneProxy` 字段 | 供 `BuildRenderQueue` 读取 |
| `SpriteQuadMesh::EnsureInitialized` | 共享几何 |
| `ComputeSpriteNeedsTranslucentPass(color, texture, material*)` | 纯函数，便于单测 |

### 9.9 数据流（端到端）

```text
[Authoring / Lua / Editor]
  SpriteComponent.{Texture, Color, Size, UVRect, Transform}
           │
           │ MarkRenderStateDirty
           v
[End of frame] PrimitiveComponent::DoEndOfFrameUpdate
           │
           v
  RenderScene::UpdatePrimitive
           │
           ├─ CreateSceneProxy / refresh SpriteSceneProxy
           │     copy Color, UV, Size, Texture*
           │     resolve Material (opaque vs translucent asset)
           │     m_bNeedsTranslucentPass = Compute...(§9.4)
           │     m_CastShadow = false
           │     bind shared SpriteQuadMesh pointers
           v
[Frame] ForwardRenderer::Execute
           │
           v
  BuildRenderQueue
           │  dynamic_cast SpriteSceneProxy*
           │  fill MeshDrawCommand (shared VB/IB, ModelMatrix, Material)
           │  push Opaque or Translucent by m_bNeedsTranslucentPass
           v
  ShadowPass  ← 仅 OpaqueQueue 中 CastShadow 者；Sprite 被跳过
  BasePass    ← Opaque（含不透明 Sprite）
  TranslucencyPass ← 半透 Sprite（按相机距离排序，与网格半透相同）
           v
  SceneColor → (future ScreenUI) → Post → Present
```

```mermaid
flowchart TD
  SC[SpriteComponent] -->|CreateSceneProxy| SP[SpriteSceneProxy]
  Q[SpriteQuadMesh 共享] --> SP
  M[Sprite Unlit Material] --> SP
  SP --> BRQ[BuildRenderQueue]
  BRQ -->|opaque| OQ[OpaqueQueue]
  BRQ -->|translucent| TQ[TranslucentQueue]
  OQ --> Base[BasePass]
  TQ --> Trans[TranslucencyPass]
  Base --> SColor[SceneColor]
  Trans --> SColor
```

### 9.10 测试与验证建议

| 用例 | 期望 |
|------|------|
| Color=(1,1,1,1)，RGB 三通道纹理 | OpaqueQueue |
| Color.a=0.5，任意纹理 | TranslucentQueue |
| Color.a=1，RGBA 纹理 | TranslucentQueue（保守） |
| 无 Texture | 不入队 |
| CastShadow 查询 | false；阴影图无该 caster |
| 旋转 Transform | 面片朝向变，无需 billboard |

单测可测：`ComputeSpriteNeedsTranslucentPass`；集成：Playground 摆两张 Sprite。

### 9.11 刻意不做（Path A）

- Sprite 资产图集编辑器、Flipbook  
- 每实例动态批到一个 mega VB（可后置）  
- Billboard / 相机锁定  
- 阴影  
- 与 UI Widget 共用 ScreenUI Pass  

---

## 10) Path B 契约（后置；数据结构预告）

与 Path A **同构思想、不同队列**：

```text
CanvasComponent : SceneComponent
  m_RenderMode = ScreenSpace | WorldSpace(后)

WidgetComponent : SceneComponent  // Image / Button 等可视节点
  → CreateSceneProxy() → WidgetSceneProxy
        → 产出 UIDrawCommand（rect, uv, color, clip, order）
        → ScreenUIQueue
        → ScreenUIPass
```

```text
UIDrawCommand  // 示意
{
  Vector4 clipRect;
  Vector4 uvRect;
  Vector4 color;
  RHITexture* texture;   // optional
  uint32_t stableOrder;
  // canvasId / batch key
}
```

**红线：** Widget Proxy **不得** `push_back` 到 `OpaqueQueue`/`TranslucentQueue`。  
命名：统一 **`WidgetComponent`**（不用 VisualWidgetComponent）。  
详细实现留待 Path A MVP 之后扩写本节或另文。

---

## 11) Status note

| 字段 | 内容 |
|------|------|
| Status | **In Progress**（Path A S00–S02 已落地并目视验收；UVRect GPU remap 后置） |
| Next | 扩写 §10 / Impl S03：ScreenUI Queue+Pass + `WidgetComponent` 最小切片 |
| Path B | 契约 §10；下一主线 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 占位：Sprite 2D |
| 2026-09-03 | 升格 Foundation；双路径修订；先 Path A；Billboard Out；Proxy 同构 Path B |
| 2026-09-03 | Color + 阴影拍板 |
| 2026-09-03 | **§9 详细设计：** 数据结构/接口/数据流；透明性 = Color.a **或** 纹理可能含 alpha；Status → Planned |
| 2026-09-04 | Path A 代码+目视验收；Status → In Progress；Next → Path B |
