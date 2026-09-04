# UI-F01 — UI System

## Meta
- **ID:** `UI-F01`
- **Type:** Feature
- **Status:** Planned（方向稿；等 `RND-F16` Path B MVP：`WidgetComponent` 可画后再扩 Canvas）
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Related:**
  - [RND-F16](../../Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)（**硬依赖**；Screen UI = Path B）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - 外部底稿：[minEngine_ui_mvp_suggestions.md](../../../external/minEngine_ui_mvp_suggestions.md)
  - UE：`UWidgetComponent` / `EWidgetSpace::{Screen,World}`
- **Branch:** `feat/ui`

## TL;DR

复用 `GameObject` + `SceneComponent` + Lua；以 **Canvas** 为 UI 根与提交单位。  
**Screen Space Canvas** 走 `RND-F16` **Path B**（Base 后、Post 前 ScreenUI Pass），**不**走 Opaque/Translucent 距离排序。  
可视节点类型名：**`WidgetComponent`**（`SceneComponent` + Proxy 同构；故意不抄 UE Screen「无 Proxy」）。  
**World Space Canvas** 后置（Path C）。  
**SpriteComponent** 属 `RND-F16` Path A，**不是** UI-F01 范围。

## Scope（方向）

### In（将来）
- `CanvasComponent` + `RenderMode`（Screen / World 预留）
- 子节点 **`WidgetComponent`** + 少量原语（Panel / Image / Text / Button 可作为其子类或组合）
- Layout → Hit-test → Events → Lua
- Screen Canvas → 填充 `RND-F16` ScreenUI Queue

### Out
- `SpriteComponent` 实现（`RND-F16` Path A）
- 完整 CSS / Slate / 第二套 Object Runtime
- Editor ImGui

## Reader quick start
1. [RND-F16 §3 / §9 / §10](../../Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)
2. 本文件 Canvas 模型
3. 实现顺序：RND-F16 Path A → Path B → 本 Feature 接线

---

## 1) Canvas 模型（方向锁定）

```text
GameObject (Canvas root)
 └── CanvasComponent          ← RenderMode, 提交/batching 单位
      └── widget GOs
           ├── WidgetComponent   ← 可视 UI 节点（可再派生 Image 等）
           │     SceneComponent.transform：xy 平面；z 层序意图
           └── LuaScript（可选）
```

| RenderMode | 渲染归属 | MVP |
|------------|----------|-----|
| **ScreenSpace** | `RND-F16` Path B · ScreenUI Pass | **先做** |
| **WorldSpace** | Path C | 后置 |

## 2) 与 Sprite 的边界

| | Sprite | Widget |
|--|--------|--------|
| Feature | `RND-F16` Path A | `UI-F01` + Path B |
| 组件 | `SpriteComponent` | `WidgetComponent` |
| 队列 | Opaque / Translucent | ScreenUIQueue |

## 3) Status note

| 字段 | 内容 |
|------|------|
| What's not | 完整 Design / 代码；**Canvas 等 Path B Widget 可画之后** |
| Unblock | `RND-F16` Path B S03b（ScreenUI 可提交） |
| Branch | `feat/ui` |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 占位 |
| 2026-09-03 | Canvas GO 方向；与 RND-F16 双路径对齐 |
| 2026-09-03 | 可视节点统一命名为 **`WidgetComponent`** |
| 2026-09-04 | 对齐 RND-F16 §10：Path B MVP **无 Canvas**；Canvas 仍属本 Feature 后续 |
