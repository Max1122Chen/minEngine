# RND-F24 — Programmable Mesh Vertex Attributes（Color → CustomN）

## Meta
- **ID:** `RND-F24`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `In Progress`
- **Owner:** project maintainer
- **Last updated:** `2026-10-16`（S01–S03 Done；S04 Pending）
- **Related:**
  - Material 消费面：`MAT-F01`–`F06`（Done）；本 Feature 扩展 shell / 图节点
  - Mesh 提交：`RND-F18` MeshBatch（Done）
  - 触媒：minCraft `ENG-GAP-MESH-VERTEX-ATTR` / TD-018（消费方之一，非体素专用 Feature）
  - 对标：UE `FColorVertexBuffer` + `LocalVertexFactory` COLOR@3；Godot `ARRAY_COLOR` / `ARRAY_CUSTOM0..3`
  - 哲学：[ENGINE_DESIGN_PHILOSOPHY](../ENGINE_DESIGN_PHILOSOPHY.md)
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Depends on:** Modern RHI + MaterialCompiler shell（已有）
- **Blocks:** 外仓程序化网格顶点色/亮度 bake 的正式材质路径

## TL;DR

RHI 已能声明任意 `RHIVertexElement`，但 **Material GLSL/SPIR-V shell 写死 P/UV/N/T**，且 Vulkan 按 layout **数组下标**绑 `location`，导致可选属性与 skinning 冲突、材质无法消费顶点色。本 Feature 建立 **语义→固定 location 契约**，先交付一等 **Color**，再交付 **Custom0..N**；材质图提供消费节点。不内置体素 LightEngine。Status：In Progress（S01–S03 **Done**；S04 工厂糖衣 Pending）。

## Reader quick start

1. 读 Problem（尤其 location 按下标绑定）。
2. 读 Proposed §Canonical locations + Material 消费。
3. 读切片：S01 契约+RHI location；S02 Color；S03 Custom。
4. Non-Goals：无 LightEngine / 无仅服务 minCraft 的特例 API。

## Problem

### 产品

- 程序化网格（体素 chunk、地图编辑器、fort mesher）需要在 CPU 写入每顶点亮度/颜色，由 Material 插值着色。  
- 今日外仓可自建 layout，但 **共享引擎材质** 读不到 Color；只能 fork shader 或放弃明暗。

### 架构

| 层 | 现状 |
|----|------|
| `RHIVertexElement` | Name + Type；无稳定 location 字段 |
| Vulkan PSO | `attribute.location = i`（元素顺序下标） |
| Material shell | 固定声明 `a_Position` / `a_TexCoord` / `a_Normal` / `a_Tangent`；skinned 硬编码 location 4/5 为骨骼 |
| 材质图 | 有 `TextureCoordinate`；**无** VertexColor / Custom |
| DebugDraw | 已有 `a_Color`（特例路径，不进产品材质） |

**核心缺陷：** 可选属性若用「稠密打包 location」，Color 与 Bone 会抢 location；不同 shading 组合下 Color 的 location 不稳定 → 程序化网格无法与材质契约对齐。

## Requirements

### 契约 / RHI

- **R1** 文档化并代码化 **Canonical Vertex Attribute Semantics**（名称 + 固定 `location` + 默认 CPU 类型）。  
- **R2** `RHIVertexElement`（或等价）携带 **显式 location**（由语义表填充；禁止再靠数组下标当语义）。  
- **R3** OpenGL / Vulkan 创建 input layout / PSO 时使用该 location。  
- **R4** 网格可只上传所用语义的子集；**未上传且材质需要的语义 → 编译/绑定时可诊断**（Warn 或 Fail，见 Failure）；不要求为每个可选语义绑 Null buffer（MVP 不做 UE 式 stride-0 null color，除非实现成本低）。

### Material

- **R5** 图节点 **VertexColor**（输出 float3/float4，可 Mask）→ shell 声明 `a_Color` @ canonical location + varying。  
- **R6** 编译环境根据图使用情况设置 `UsesVertexColor` / `UsesCustom[i]`；shell **按语义 location 声明**，不按「出现顺序重编号」。  
- **R7** 可选工厂：`CreateUnlitVertexColorMaterial`（或文档化最小图：TextureSample × VertexColor → Albedo）降低外仓成本——工厂是糖衣，不是唯一路径。

### Custom（同 Feature，可后切片）

- **R8** `Custom0..Custom3`（MVP 至少 **Custom0..1**）语义 + location；格式首期 **Float / Float2 / Float4**。  
- **R9** 图节点 **VertexCustomAttribute**（选 index + 组件数）或等价。  
- **R10** `UByte4Norm` 等打包格式 **可后置切片**，Design 预留枚举位，不挡 Color/Custom float。

### 验收

- **R11** 金样：CPU 填灰度/渐变 `a_Color` → 画面可见明暗；GL 与 VK（若本机 VK 可用）不崩。  
- **R12** 现有无 Color 的 StaticMesh / Sprite / 默认 Blinn 路径 **零回归**（未使用新节点则 shell 不声明 Color）。  
- **R13** Skinned：骨骼语义迁到 canonical 高位 location；现有骨骼材质/网格同步更新（本 Feature 内完成，不留永久双轨）。

## Constraints

- 保持 **interleaved 单 vertex buffer** 为主路径（不强制抄 UE 多 stream；多 stream 后置）。  
- 主线程上传；不引入异步 mesh cook。  
- 不把 LightEngine / chunk nibble / 刷怪光规则放进 Core。  
- Forward-only：location 表变更允许一次性改皮肤 location。

## Preferences

- 语义名保持现有前缀：`a_Position`, `a_TexCoord`, `a_Normal`, `a_Tangent`, `a_Color`, `a_Custom0`…  
- TexCoord 首期仍单套 `a_TexCoord` @1；多 UV 后置为 `a_TexCoord1`…  
- 材质缺省不乘 VertexColor（须图显式连接），避免静默改变所有材质。

## Assumptions

- 程序化网格作者（游戏仓）能按 canonical 语义填数据；引擎提供表 + 辅助常量/枚举。  
- SPIR-V 反射/汇编路径与 GLSL shell 使用同一 location 表。

## Non-Goals

- 引擎内置体素光照传播 / 日夜天空光场。  
- 完整复制 Godot 全部 `ARRAY_CUSTOM_*` 格式矩阵（首期 float 族足够）。  
- Per-instance CustomData（UE Instance Custom Data）——另一 Feature。  
- 自动把「无 Color 网格」升级为带 Color（不静默扩 stride）。  
- 专用于 minCraft 的 `ChunkLight` API 命名。

## Current Architecture

```text
StaticMesh(vertices, elements[]) → RHICreateBuffer + RHICreateVertexInputLayout(elements)
MaterialCompiler → GLSLMaterialShellAssemblerImpl::BuildVertexIoBlock
  location 0 Position, 1 UV, 2 Normal, 3 Tangent, [4/5 bones if skinned]
Vulkan: location = element index in vector  // 脆弱
```

minCraft：`WorldRenderer` 共享 layout P/UV/N/T，无 Color；Chunk proxy 自建 buffer。

## Proposed Architecture

### 1) Canonical location table（锁定草案）

| Location | Semantic | Name | Default type | Notes |
|----------|----------|------|--------------|-------|
| 0 | Position | `a_Position` | Float3 | 必需 |
| 1 | TexCoord0 | `a_TexCoord` | Float2 | 常用 |
| 2 | Normal | `a_Normal` | Float3 | 光照模型需要时 |
| 3 | Tangent | `a_Tangent` | Float4 | TBN |
| 4 | Color | `a_Color` | Float4 | **本 Feature 一等公民** |
| 5 | Custom0 | `a_Custom0` | Float4（可降维） | |
| 6 | Custom1 | `a_Custom1` | Float4 | |
| 7 | Custom2 | `a_Custom2` | Float4 | 可后置启用 |
| 8 | Custom3 | `a_Custom3` | Float4 | 可后置启用 |
| 9 | BoneIndices | `a_BoneIndices` | Int4 | **从旧 4 迁出** |
| 10 | BoneWeights | `a_BoneWeights` | Float4 | **从旧 5 迁出** |

规则：

- Shader `layout(location = N)` **永远**用上表，不因「未使用 Normal」而把 Color 挪到 2。  
- CPU interleaved buffer 的 **元素顺序可与 location 不同**，但每个 element 必须带正确 location；stride/offset 按实际打包计算。  
- 推荐作者打包顺序与 location 递增一致，降低心智负担（文档示例按 0..4 顺序）。

### 2) RHI

```text
RHIVertexElement {
  Name, Type, bNormalized, Offset,
  Location  // NEW: uint32；或由 Name→表查询在 CreateVertexInputLayout 时解析
}
```

- `RHICreateVertexInputLayout`：Vulkan `attribute.location = element.Location`；GL `glVertexAttribPointer` 绑到 Location。  
- 提供 `VertexAttributeLocations::Color` 等常量，避免魔数。

### 3) Material shell / 图

```text
图使用 VertexColor?
  → env.UsesVertexColor = true
  → VS: layout(location=4) in vec4 a_Color; out vec4 v_Color;
  → FS: in vec4 v_Color; 节点输出 v_Color
```

Custom 同理 location 5+。

与 lighting varyings 的 **out location** 编号仍可稠密打包（与 vertex **in** location 独立）——保持今日 out 递增策略即可。

### 4) 程序化网格推荐路径

```text
1. 填 CPU 顶点（含 Color 或 Custom0）
2. RHICreateVertexInputLayout({
     {Position, loc0}, {TexCoord, loc1}, {Normal, loc2}, {Tangent, loc3}, {Color, loc4}
   })
3. 上传 VB；SceneProxy 提交 MeshBatch（与今日相同）
4. 材质图连接 VertexColor → Albedo（或 × TextureSample）
```

### 5) 对标映射

| 引擎 | 做法 | minEngine |
|------|------|-----------|
| UE | 独立 Color stream + VF 固定 attr 3；缺色用 null | 一等 Color @4；MVP 不强制 null bind |
| Godot | ARRAY_COLOR + CUSTOM0..3 格式枚举 | Color + Custom0..3；格式先 float |
| Unity | Mesh.colors / VertexAttribute | 同语义表 |

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| Vertex semantic table / docs | 稳定 location 契约 | 游戏 bake 公式 |
| RHI input layout | 按 Location 绑定 | 材质图编辑 UX |
| MaterialCompiler shell + nodes | 声明/插值/节点 IR | 体素光传播 |
| StaticMeshLoader / Sprite / Skin | 迁移到新 location（骨骼） | 外仓 chunk mesher |
| 游戏仓 | 填顶点数据；选材质图 | 改引擎 shell |

## Boundaries

- **Allowed：** 通用语义扩展；材质可选消费。  
- **Forbidden：** `LightEngine` 进 Runtime Core；仅 minCraft 可见的引擎 API。  
- **MAT 域：** 图节点与 shell 变更记在本 Feature；不单开 MAT-F07，除非审批后拆分。

## Dependencies

- Depends on: RHI buffers、MaterialCompiler、Skeletal layout 调用点。  
- Depended on by: 程序化网格游戏、未来地形/调试着色。  
- 与 CORE-F36 **无依赖**，可并行。

## Ownership & Lifetime

- Layout / VB：与今日 StaticMesh / 游戏 proxy 相同（创建者拥有）。  
- 语义表：引擎静态常量，无运行时所有权问题。

## Data Flow

```text
CPU vertex bytes → VB
Semantic elements(+Location) → VertexInputLayout
Material graph → Uses* flags → shell in/out @ fixed locations
Draw: PSO vertex input matches shader locations
```

## Control Flow

编译期：MaterialCompiler 扫图 → 设 flags → 生成 VS/FS。  
运行时：上传网格；Draw 绑 layout+PSO（无每帧重写语义表）。

## State & Invariants

- I1：同一语义永远同一 location（跨 GL/VK、跨材质）。  
- I2：未 `UsesVertexColor` 的材质 **不**声明 `a_Color`（避免强求网格带 Color）。  
- I3：声明了 `a_Color` 的材质与缺 Color 的 layout 组合 → 诊断（见 Failure），不静默读垃圾。  
- I4：骨骼 mesh 使用 location 9/10；旧 4/5 骨骼路径删除。

## API / Interface Semantics

### 语义 API（示意）

```text
namespace VertexAttr {
  enum class Semantic : uint8_t { Position, TexCoord0, Normal, Tangent, Color, Custom0, ... BoneIndices, BoneWeights };
  uint32_t Location(Semantic);
  const char* Name(Semantic);
  VertexElementType DefaultType(Semantic);
  RHIVertexElement Make(Semantic, VertexElementType overrideType = Default);
}
```

### 材质图

| 节点 | 输出 | 说明 |
|------|------|------|
| `VertexColor` | float4（可接 ComponentMask） | 读 `v_Color` |
| `VertexCustom` | float4 | 属性：CustomIndex 0..3 |

### 工厂（可选糖衣）

- `SpriteMaterialFactory` 旁或 `VertexColorMaterialFactory::CreateUnlit(...)`：最小可运行 Unlit × VertexColor。

### Errors

- 未知 Semantic / 重复 location：CreateVertexInputLayout Fail + Log。  
- 材质 UsesColor 但 layout 无 Color：Draw 前或 Compile 后 Warn；Debug 可 Assert。

## Failure Model

| 模式 | 行为 |
|------|------|
| 网格缺材质所需语义 | Warn + 跳过 draw 或用默认（**锁定：Warn + skip draw for that batch**，避免花屏；可配置后续） |
| 重复 location | Fail layout 创建 |
| 旧资产骨骼 location | 本 Feature 迁移代码路径；无旧 PSO 缓存长期并存 |

## Alternatives & Trade-offs

### Option A — Canonical fixed locations + interleaved（推荐）

- Pros：可选属性不挤占；与 shader 契约稳定；可演进 Custom。  
- Cons：须迁移骨骼 location；RHI 两后端都改。

### Option B — 继续稠密打包，Color 仅出现在「特定材质变体」末尾

- Pros：改动面小。  
- Cons：Color location 随是否 Tangent/Skin 变化；程序化网格易绑错；**拒绝**作为终态。

### Option C — UE 式多 stream + null color bind

- Pros：缺色不改 stride。  
- Cons：RHI/绑定复杂度高；超出现阶段收益。  
- Why not for MVP：后置增强。

## Integration Impact

- `RHIBuffers.h`、OpenGL/Vulkan VertexInputLayout、PSO 创建。  
- `GLSLMaterialShellAssemblerImpl`（及 VK/SPIR-V 等价路径）。  
- SkeletalMesh 上传与 shell skinned 分支。  
- `StaticMeshLoader`、`SpriteQuadMesh`：核对 location（通常仍 0..3，无 Color 则不变）。  
- Material 图节点注册 + Editor 调色板（若节点需进编辑器列表）。  
- 测试：render/material suite 或新 `vertex-attributes` 金样。

## Migration / Compatibility

- 无 Color 的网格+材质：行为不变。  
- **Skinned：破坏性 location 变更**，在本 Feature 内改完引擎侧所有引用；游戏仓若自建 skinned layout 须跟表（文档公告）。  
- minCraft：自行在 layout 加 Color @4 并换材质图——引擎不代写 LightEngine。

## Verification Strategy

- **契约测试：** Semantic→Location 表稳定（编译期或 suite 断言常量）。  
- **金样：** 程序化四边形/立方体顶点色梯度；Unlit+VertexColor；GL（必选）/VK（若 CI 有）。  
- **回归：** Sprite、默认 StaticMesh、Skeletal 一帧 smoke / 现有 animation 相关 suite。  
- **负例：** UsesColor 材质 + 无 Color layout → Warn 且不崩。

## Suggested slices

| Slice | Goal | Verify | Status |
|-------|------|--------|--------|
| S01 | Location 字段 + GL/VK 按 Location 绑定；表常量；骨骼迁 9/10 | suite `vertex-attribute-semantics` + mesh/skin 回归 | **Done** |
| S02 | Shell `a_Color` + VertexColor 节点 + 编译金样 | `material-ir` VertexColor；无 Color 材质零回归 | **Done**（画面梯度金样可后置） |
| S03 | Custom0..1 + VertexCustom 节点 | 金样读 Custom0 | **Done** — [S03 Design](./RND-F24_S03_VERTEX_CUSTOM_DESIGN.md) |
| S04 | 工厂糖衣 + 外仓文档段落 | 手册/Progress | Pending |

`UByte4Norm`：S05 或下 Feature。

## Open Questions

| Q | Blocking? | Resolution |
|---|-----------|------------|
| 缺 Color 时 skip draw vs 默认白？ | no | **锁定 MVP：Warn + skip batch**；默认白可后置（更接近 UE null） |
| Custom2/3 首期是否启用？ | no | 表占位；S03 先 0..1 |
| TexCoord1 何时？ | no | 后置；不挡本 Feature |
| 是否拆 MAT-F07？ | no | **不拆**；节点含在 RND-F24 |

## Design Review

- **Verdict:** `Ready with deferred items`（自审 2026-10-15；待维护者审批升 Planned）  
- **Reviewer / date:** agent self-review / 2026-10-15  
- **Summary:** 见会话报告。关键正确性点（下标 location）已写入契约；null-color 与 UByte4Norm 后置合理。

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved
- [x] Major state/invariants resolved
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Open questions deferred/accepted

**Ready for implementation?** `yes` — 维护者指示开工；在 `feat/render` 实现。

## Acceptance Checklist

- [x] Design review 维护者确认（指示继续 mesh vertex feat）
- [ ] Implementation plan traces to design（L2；按 Design slices 直落）
- [x] Progress log when status changes
- [x] Feature registry status synced
- [ ] 维护者验收 feat 后合 master

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-15 | 初稿：canonical location；Color→Custom；RHI Location；骨骼迁位；材质节点 |
| 2026-10-15 | S01：`VertexAttr` 表；`RHIVertexElement::Location`；GL/VK 绑定；骨骼 shell @9/10 |
| 2026-10-15 | S02：VertexColor 节点（RGBA/RGB）；shell `a_Color@4`/`v_Color`；material-ir 覆盖 |
