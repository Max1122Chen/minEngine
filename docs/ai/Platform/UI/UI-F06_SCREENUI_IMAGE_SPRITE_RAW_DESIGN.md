# UI-F06 — ScreenUI Image：Sprite / Raw 源

## Meta
- **ID:** `UI-F06`
- **Type:** `Feature`
- **Complexity:** `L1`（若把 FillAmount 塞进同一 Feature 则偏 L2；默认 L1，Fill 另开 F07）
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-10-09`
- **Branch:** `feat/ui`（工作树 `D:/Dev/GitRepo/minEngine-ui`）
- **Related:**
  - [UI-F01](./UI-F01_UI_SYSTEM_DESIGN.md)（Image = 视觉；Widget = 几何）
  - [UI-F05](./UI-F05_SCREENUI_DRAG_DROP_DESIGN.md)（拖影图标）
  - Sprite 运行时：`Sprite` / `SpriteComponent`（`GetUVRectNormalized`、`GetRuntimeTexture`）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - 外部驱动：minCraft 背包/物品图标吃图集（愿景「Image UV/Sprite」强期望）
- **Depends on:** `UI-F01` Done；Sprite 资产路径 Done（`SPR-*`）
- **Blocks:** 无需为每个图标单独 Texture2D 即可做 atlas 图标 HUD

## TL;DR

扩展 `ImageComponent`，支持两种源模式：**Raw**（整张 `Texture2D`，UV 0–1）与 **Sprite**（按 `Sprite::GetRuntimeTexture` + `GetUVRectNormalized` 采样）。  
解析方式对齐现有 `SpriteComponent`。把 UV 接到 `WidgetComponent` → `WidgetSceneProxy` / ScreenUI 材质（今日写死全矩形 UV）。

## Problem

- `ImageComponent` 目前只有 `Texture2D` + tint。
- `WidgetComponent::FillSceneProxy` / `SyncMaterialParameters` 强制 `m_UVRect = (0,0,1,1)`，且 `ApplyColorAndTexture` 不带 UV 参数。
- ScreenUI 材质图源自 Sprite 路径，世界 Sprite 已有 UVRect 标量参数；ScreenUI factory 尚未暴露。
- minCraft 需要从方块/物品图集取图标，不能为每个 icon 烤一张 Texture2D。

## Requirements

- 创作 / 运行时可选择：
  - **Raw：** 绘制整张纹理（当前行为）。
  - **Sprite：** 使用 Sprite 运行时纹理 + Sprite UV 矩形。
- Resolve API：`ResolveDrawTextureShared()` / `ResolveDrawUVRect()`（同 `SpriteComponent` 思路）。
- Sprite 模式用 Sprite UV；Raw 模式用全矩形（可选 Raw UV 覆盖见 Preferences）。
- 源或 tint 变化时 dirty 同级 Widget（沿用 `NotifySiblingWidgetDirty`）。
- Editor：Mode + Texture / Sprite 字段可见；非法组合有文档约定。
- 测试：Raw 全图；Sprite UV 与 `Sprite::GetUVRectNormalized` 一致；Sprite null 安全；Button TargetGraphic tint 仍可用。

## Constraints

- Image 仍只做 **视觉**；Hit 仍在 `WidgetComponent`。
- 不另造 UI 专用图集格式；复用 `Sprite` / `SpriteAtlas`。
- 不把世界空间 `SpriteComponent` 拉进 ScreenUI 树；只共享 **资产 + UV 数学**。
- UV 空间必须与现有 Sprite/ScreenUI 着色约定一致（OpenGL 风格 u0,v0,u1,v1；v=0 在底）— 直接用 `GetUVRectNormalized()`，禁止第二套转换。
- 前向兼容：仅有 `m_Texture` 的旧场景必须仍按 Raw 工作。

## Preferences

- 显式枚举 `EUIImageSourceMode { Raw, Sprite }`（Inspector 比「Sprite 非空即 Sprite」更清晰）。
- 配置错误时的 Resolve：
  - Mode=Sprite 且 Sprite null → 不绘制（或白 1×1）+ 可选一次 warn；**不**静默退回 Raw Texture。
  - Mode=Raw 且 Texture null → 与今日 factory 行为一致（白/不可见）。
- 可选 Raw `UVRect` 覆盖（**Should**，非 MVP 必做）：无 Sprite 时也可裁 UV；若本期不做，归 **UI-F07 FillAmount**。
- 命名对齐 `SpriteComponent::{SetSprite,SetTexture,ResolveDraw*}`。

## Assumptions

- `ScreenUIMaterialFactory` 可增加 `ApplyColorTextureAndUVRect`（或在材质图兼容时复用 `SpriteMaterialFactory::ApplyColorTextureAndUVRect`）。实现前核实；图不兼容则只扩 ScreenUI factory。
- HUD 常见路径是 Atlas Rebuild 后的 Baked Sprite；未 bake 的 Source Sprite 仍走 `GetRuntimeTexture` 既有规则。

## Non-Goals

- 九宫格 / 边框 Image（另候选）。
- Mask / stencil 软裁剪。
- 动图 Flipbook Image（可用换 Sprite 帧 / 另 Feature）。
- 自动把 UI 贴图打进「仅 UI」图集。
- 改变 Button 命中规则。

## Current Architecture

```text
ImageComponent: m_Texture + m_Color
WidgetComponent::SyncMaterialParameters → ScreenUIMaterialFactory::ApplyColorAndTexture（无 UV）
WidgetComponent::FillSceneProxy → proxy.m_UVRect = (0,0,1,1); proxy.m_Texture = image->GetTexture()
```

`SpriteComponent` 已有：

```text
ResolveDrawTexture: Sprite 运行时，否则 m_Texture
ResolveDrawUVRect: Sprite GetUVRectNormalized，否则 m_UVRect
SpriteMaterialFactory::ApplyColorTextureAndUVRect(...)
```

## Proposed Architecture

### 3.1 ImageComponent 字段

```text
enum class EUIImageSourceMode : uint8_t { Raw = 0, Sprite = 1 };

ME_PROPERTY Mode / SetSourceMode
ME_PROPERTY Texture（Raw）
ME_PROPERTY Sprite（Sprite 模式）
ME_PROPERTY Color（既有）
```

公开辅助：

```text
void SetSourceMode(EUIImageSourceMode mode);
void SetTexture(...);   // 不自动改 Mode（见下）
void SetSprite(...);    // 不自动改 Mode；调用方设 Mode=Sprite

std::shared_ptr<Texture2D> ResolveDrawTextureShared() const;
Vector4 ResolveDrawUVRect() const; // (u0,v0,u1,v1)
bool CanDraw() const;
```

**Mode 切换策略（推荐）：** 普通 Setter **不**自动改 Mode（避免序列化意外）。Editor / 便利 API 可提供 `SetRawTexture` / `SetSpriteSource`，一次设置 mode + 资产。

### 3.2 Resolve 规则

| Mode | Texture | Sprite | 绘制纹理 | UV |
|------|---------|--------|----------|----|
| Raw | 非空 | * | Texture | (0,0,1,1) |
| Raw | null | * | 白 / 无（既有） | (0,0,1,1) |
| Sprite | * | 非空 | `Sprite::GetRuntimeTextureShared()` | `GetUVRectNormalized()` |
| Sprite | * | null | 无 | 退化 UV；跳过绘制 |

### 3.3 Widget / 材质接线

- `FillSceneProxy`：`proxy.m_Texture = image->ResolveDrawTexture()`；`proxy.m_UVRect = image->ResolveDrawUVRect()`。
- `SyncMaterialParameters`：应用 color + texture + **UVRect** 标量。
- 扩展 `ScreenUIMaterialFactory`（优先），或在图兼容时复用 Sprite apply helper。

### 3.4 Button TargetGraphic

- Tint 仍写 `ImageComponent::SetColor` / ApplyTint — 不变。
- Hit 不变。

### 3.5 相关后续（本期考虑、非本 MVP）

| 候选 | 为何相关 | 建议 |
|------|----------|------|
| **UI-F07 FillAmount** | 炉进度条 / 血条类 | 0–1 横向/纵向填充；可建在 Raw UV 裁剪上 |
| Nine-slice | 面板边框 | Deferred；需 mesh 或 9 四边形 |
| PreserveAspect | 槽内图标 letterbox | Should；小旗标即可 |
| RaycastPadding | 槽位点按放大 | Deferred |
| Image 材质覆盖 | 花式 HUD | Deferred；保持 factory 实例 |

## Responsibilities

| 单元 | 负责 | 不负责 |
|------|------|--------|
| `ImageComponent` | Mode、资产、ResolveDraw* | Hit、布局 |
| `WidgetComponent` | 把 resolve 推进 proxy/材质 | 拥有 Sprite 资产生命周期政策 |
| `ScreenUIMaterialFactory` | ScreenUI 的 UV 参数应用 | Atlas Rebuild |
| `Sprite` | UV 数学 / 运行时纹理 | ScreenUI 布局 |
| 游戏 | 按物品 id 选 Sprite | 引擎 Inventory |

## Boundaries

- 允许：Image → 引用 Sprite 资产（shared_ptr）；Widget 读 Image resolve。
- 禁止：Image 依赖 SpriteAtlas 打包 API；ScreenUI 引入世界 `SpriteComponent`。
- 禁止：Image 出现游戏 `ItemDef` 类型。

## Dependencies

- Depends on：UI-F01；Sprite `GetUVRectNormalized` / runtime texture。
- Depended on by：minCraft HUD 图标；UI-F05 拖影观感；UI-F07 fill。

## Ownership & Lifetime

- `shared_ptr<Texture2D>` / `shared_ptr<Sprite>`，与今日 / SpriteComponent 一致。
- 无新子系统。

## Data Flow

```text
创作：Mode + Texture 或 Sprite
  → ImageComponent::ResolveDraw*
  → WidgetComponent::FillSceneProxy / SyncMaterialParameters
  → ScreenUI 队列 / 绘制
```

## Control Flow

- 静态图标无需 Tick；Set* 时 dirty（与今日一致）。
- 若运行时 Atlas Rebuild 更新 Sprite：若无通知机制，MVP 约定由游戏 Rebuild 后重新 SetSprite / MarkDirty（见 Open Questions）。

## State & Invariants

- Mode=Sprite ⇒ 采样忽略 Raw Texture（Texture 字段仍可序列化以便切模式）。
- 退化 UV ⇒ 不可见（复用 `SpriteComponent::IsUVRectDegenerate`）。
- Color alpha 仍与纹理 alpha 相乘（F01 契约）。

## API / Interface Semantics

### `SetSourceMode`

- 责任：改本地 Mode；通知 Widget dirty。
- 错误：无异常；错误配置在 resolve 时处理。

### `ResolveDrawUVRect`

- 同步；返回 Vector4；不抛。
- Sprite 模式且 Sprite null → 返回退化 UV。

## Failure Model

| 失败 | 行为 |
|------|------|
| Sprite 模式、Sprite null | 不绘制；可选日志 |
| Sprite 运行时纹理 null | 不绘制 |
| 材质缺少 UV 参数 | 在扩展 factory 时检出并修图 — 切片阻塞项 |
| 旧场景仅 Texture | Mode 默认 Raw；继续工作 |

## Alternatives & Trade-offs

### Option A（推荐）— 显式 Mode 枚举 + Sprite/Texture 双字段

- 优点：符合「同时支持 Sprite 与 Raw」；Inspector 清晰；对齐 SpriteComponent resolve。
- 缺点：两字段；非活跃字段闲置。

### Option B — Sprite 非空即 Sprite 模式（无枚举）

- 优点：属性更少。
- 缺点：切换时难保留双引用；Editor 语义弱。

### Option C — 仅 Raw UVRect（不引 Sprite）

- 优点：改动极小。
- 缺点：游戏重复算 UV；与用户需求不符。

## Integration Impact

- `ImageComponent.h/.cpp`、反射 gen。
- `WidgetComponent` proxy sync。
- `ScreenUIMaterialFactory`（+ 可能材质图共享）。
- Button tint 路径需回归。
- 文档：F01 Out「Sprite UV」由本 Feature 关闭。

## Migration / Compatibility

- 默认 `Mode = Raw`；既有资产不变。
- 新字段默认安全则无需磁盘版本 bump。

## Verification Strategy

- 单测：上表 Resolve；已知 rect 时 UV 等于 Sprite helper。
- 渲染冒烟：Canvas 上 Raw vs Sprite 并排。
- 回归：Button tint；同级有 Text 时仍优先 Text 绘制路径。

## Open Questions

1. **SetSprite/SetTexture 是否自动切 Mode？** — 默认 **否**；提供便利 Setter。 — blocking? no  
2. **Atlas Rebuild 后 Image 如何 dirty？** — 需通知还是游戏刷新？ — blocking? **soft**（MVP：游戏 Rebuild 后重绑）  
3. **Raw UVRect 放 F06 还是 F07？** — 默认 **跟 F07 FillAmount**。 — blocking? no  
4. **PreserveAspect 是否进 F06？** — 默认 **out**；日后 Should。 — blocking? no  

## Design Review

- **Verdict:** `Approved`（会话内确认后实现）
- **Reviewer / date:** maintainer / 2026-10-09
- **Link or summary:** Mode 枚举 + factory UV；Raw UVRect / FillAmount 留给 F07

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved or N/A
- [x] Major state/invariants resolved or N/A
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered where meaningful
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred/accepted

**Ready for implementation?** `yes`（已实现）

## Acceptance Checklist

- [x] Design review complete when required
- [x] Implementation plan traces to design (if sliced)
- [x] Progress log updated when status changes
- [x] Feature registry status synced

## Slices

| Slice | 内容 | Status |
|-------|------|--------|
| S01 | Mode + Sprite/Texture 字段 + Resolve* + Widget proxy UV + factory UV 应用 | **Done** |
| S02 | 测试 + Editor 属性可见性说明 | **Done**（suite；Editor 说明见 Design） |
| S03 |（可选）便利 `SetRawTexture` / `SetSpriteSource` | Deferred |

## UI-F07 占位说明

**UI-F07 — ScreenUI Image FillAmount**（Planned）：0–1 填充进度（炉等）。建议 F06 Review+ 后再开 Design。本 Feature 不实现。
