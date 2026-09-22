# SPR-F03 — Sprite Runtime Consume（Component + UV→GPU）— Design Spec

## Meta
- **ID:** SPR-F03
- **Type:** Feature
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - 硬前置：[SPR-F02](./SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md)（**Done**）
  - Roadmap：[SPR-F01](./SPR-F01_SPRITE_SYSTEM_ROADMAP.md)
  - 底座：[RND-F16](../Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)
  - 入队：[RND-F18](../Render/RND-F18_MESH_BATCH_PASS_MDC_REFACTOR_DESIGN.md)

## TL;DR

**问题：** `SpriteComponent` 只认裸 `Texture2D`；`UVRect` 存了但 GPU 未 remap；无法消费 Sprite 资产的 Baked 图集。

**方案：** 组件优先持 `Sprite`；每帧/dirty 时用 Runtime API 填 Proxy；打通 Material **UVRect remap**（4 scalars + Lerp + MakeFloat2）；保留裸 Texture 路径作兼容。

**状态：** Done。

## Scope
- **In:**
  - `SpriteComponent`：`m_Sprite` 属性；解析 Runtime Texture + UV  
  - GPU：共享 unit quad + uniform/参数 UV remap（RND-F16 原议）  
  - Proxy / MeshBatch 路径填正确 UV 与纹理  
  - 半透明规则继续基于 Runtime 纹理通道 + Color  
- **Out:**
  - Atlas Rebuild / Editor（F02/F04–F06）  
  - Flipbook 自动换帧  
  - 强制删除裸 Texture API（可标文档 Deprecated，代码可留）  

## Reader quick start
1. §3 组件契约  
2. §4 UV→GPU  
3. §8 验收  

---

## Problem

资产层（F02）解决「有什么画」，本 Feature 解决「场景里怎么画对」：同 Atlas 多 Sprite 必须共享一张纹理、不同 UV，且与 MeshBatch 入队一致。

## Requirements

- 组件可指定 `Sprite`；保存/反射可见  
- 绘制使用 `GetRuntimeTexture` + `GetUVRectNormalized`  
- 非默认 UVRect 在画面上可见（相对整张纹理裁切）  
- 无 Sprite 时：保留现有 Texture + `m_UVRect` 行为（F03 起 `m_UVRect` 必须真正生效）  
- CastShadow 仍默认 false；透明分流规则不回退  

## Constraints

- 不每 Sprite 新建 VB（RND-F16 不变量）  
- 走现有 Sprite unlit Material / MeshBatch 路径  
- 依赖 F02 查询 API  

## Preferences

- UV remap 优先 **shader/参数**，非改 VB  
- Size 仍由组件 `m_Size` 控制（Sprite 可后置提供默认像素尺寸→世界，MVP 可不读）  

## Assumptions

- Sprite unlit 图可增加 UV rect 参数（或等价 Constant）而不破坏 Opaque/Translucent 双 Material  

## Non-Goals

- Billboard；合批 instance buffer；阴影  

## Current Architecture

- `SpriteComponent`：Texture / Color / Size / UVRect；`SpriteMaterialFactory`；`SpriteSceneProxy` 已有 `m_UVRect` 字段但绘制未用  
- `SpriteQuadMesh`：unit quad UV 0..1  

## Proposed Architecture

```text
SpriteComponent
  m_Sprite optional
  m_Texture / m_UVRect   // legacy / override when no Sprite
  ResolveDraw():
    if m_Sprite:
      tex = Sprite.GetRuntimeTexture()
      uv  = Sprite.GetUVRectNormalized()
    else:
      tex = m_Texture
      uv  = m_UVRect
    SyncMaterial(tex, color); Proxy.m_UVRect = uv
```

### Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `SpriteComponent` | 选择 Sprite vs legacy；dirty；填 Proxy | Pack |
| `SpriteSceneProxy` / Batch | 携带 UVRect 到 draw | 资产加载 |
| Sprite material / VS | 应用 UV remap | 知道 Atlas |

### Boundaries

- 组件不调用 `Atlas.Rebuild`  
- 渲染不写回 Sprite 资产  

## Dependencies

- Depends on: F02 Runtime API；RND-F16/F18  
- Depended on by: 手验、F05/F06 预览（可选）  

## Ownership & Lifetime

- `m_Sprite`：`shared_ptr` 或资产引用，对齐其他组件持资产方式  
- Runtime 纹理指针非拥有  

## Data Flow

`Sprite asset / legacy fields → ResolveDraw → Material params + Proxy → MeshBatch → Pass`

## Control Flow

- `SetSprite` / Source 变更 → `MarkRenderStateDirty` → EndOfFrame SyncSceneProxy  
- Atlas Rebuild 后若组件已引用该 Sprite：依赖资产 dirty / 重新 Sync（Open：监听 vs 用户重进 Play）  

## State & Invariants

- 有 Sprite 时，绘制 UV/纹理以 Sprite Runtime 为准（忽略 legacy Texture，或 Inspector 隐藏 legacy）  
- `m_Size <= 0` 不入队（保持 RND-F16）  

## API / Interface Semantics

- `SetSprite(shared_ptr<Sprite>)` / `GetSprite`  
- 现有 `SetTexture` / `SetUVRect` 保留；当 `m_Sprite != null` 时 SetTexture 行为：**推荐拒绝或仅当 ClearSprite**（Open）  

## Failure Model

- Sprite Runtime 纹理 null → 不入队 + Warning  
- UV 退化（零面积）→ 不入队  

## Alternatives & Trade-offs

### UV remap 落点
- **A 推荐：** Material 向量参数 `UVRect` + VS/图节点 remap  
- B：每 draw push constant —— 若引擎已有 per-draw 常量更好，否则 A  

### 组件是否强制 Sprite
- MVP **不强制**；双路径  

## Integration Impact

- `SpriteMaterialFactory` / 默认图增加 UV 参数  
- Proxy / MeshPassProcessor Sprite 路径传 UV  
- Inspector：Sprite 槽  

## Migration / Compatibility

- 旧场景仅 Texture：行为增强为 UVRect 生效  
- 文档：推荐新内容用 Sprite 资产  

## Verification Strategy

- 单测：ResolveDraw 在有/无 Baked 时 UV 正确（可测纯函数）  
- Editor：两 Sprite 同 Atlas，画面不同区域；legacy Texture+UVRect 裁切可见  
- 回归：无 Sprite 的既有 Sprite 场景仍可画  

## Open Questions

1. Sprite 持有时是否禁用 legacy Texture 字段编辑 — **blocking?** 否  
2. Atlas Rebuild 后已加载组件如何刷新 — dirty 通知 vs 重载 — **blocking?** 否（MVP：Mark dirty on asset save）  
3. V 翻转与 F02 同一约定 — **Resolved**（F02 `GetUVRectNormalized` top-left→OpenGL V flip）  

## Design Review

- **Verdict:** Accepted（实现合入）  

## Implementation Readiness

- [x] Responsibilities / API sketched  
- [x] Q3 resolved via F02  
**Ready for implementation?** `done`

## Acceptance Checklist

- [x] Design / code review complete  
- [x] Registry / Progress synced  

---

## Changelog

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft |
| 2026-09-22 | In Progress：`m_Sprite` + ResolveDraw*；Material UVRectU0–V1 remap；`MakeFloat2`；suite `sprite-runtime-consume` |
| 2026-09-22 | Done |
