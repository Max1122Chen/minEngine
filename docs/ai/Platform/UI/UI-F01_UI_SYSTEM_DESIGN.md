# UI-F01 — UI System

## Meta
- **ID:** `UI-F01`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Branch:** `feat/ui`
- **Related:**
  - [RND-F16](../../Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)（ScreenUI Pass / `WidgetComponent` 基础）
  - [CORE-F08](../Core/CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md) / [CORE-F09](../Core/CORE-F09_PARALLEL_HIERARCHY_KEEPWORLD_DESIGN.md)（UI Tree = GO 父子）
  - [ED-F05](../../Editor/ED-F05_HIERARCHY_TREE_DESIGN.md)（树形 Hierarchy + 拖拽改父）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - 外部底稿：[minEngine_ui_mvp_suggestions.md](../../../external/minEngine_ui_mvp_suggestions.md)
- **Depends on:** `RND-F16` Path B Done；`CORE-F08` / `CORE-F09` / `ED-F05` Done
- **Blocks:** Hit-test、Text/Button、完整 Flex、World Canvas
- **Implementation:** （本 Feature 内一次性落地 MVP；无单独 Impl 切片文件）

## TL;DR

在已有 ScreenUI（Path B）之上，建立 **Canvas + Widget Layout（Anchor/Margin/Size + Preset）+ Image**。  
**`WidgetComponent` = ScreenUI 基础节点**：几何 / 序 / 可见 / Layout；不承载贴图样式。  
**`ImageComponent` = 第一款视觉**（Texture + Tint）。  
UI Tree = **GO 父子**。绘制几何来自 **Computed Rect（参考分辨率空间）→ Canvas Letterbox → 视口像素**，不依赖 GO world Transform。

## Scope

### In（MVP）
- `CanvasComponent`（ScreenSpace）：参考分辨率、**Letterbox** 适配、子树 UI 根
- Layout 在 `WidgetComponent`：固定 Size + AnchorMin/Max + Margin → Computed Rect
- **`EUIAnchorPreset` + `ApplyAnchorPreset`**：编辑器选预设写入底层 Anchor（Unity/UE 同构）
- `ImageComponent`：Texture × Color；几何来自同 GO Widget
- 仅 Canvas 子树内、Widget+Image 成对节点入 `ScreenUIQueue`

### Out
- Text / Button / Panel 专用组件
- 完整 Flex/Grid；Hit-test / Focus / Input
- World Space Canvas；Stretch Scale Mode（枚举可预留，MVP 不实现）
- Editor ImGui UI；第二套 Object Runtime

## Reader quick start
1. §3 组件分层与数据流  
2. §3.3–3.4 Anchor / Preset  
3. RND-F16 §10（ScreenUI）

---

## 1) 背景与目标

**Pain：** Path B 能画单个 Widget，但无 UI 根、无相对布局、无「图片」语义。

**成功标准（MVP）：**
- 场景中有 Canvas GO；其子 GO 树下挂 Widget+Image，按参考分辨率 Layout 后经 Letterbox 在 ScreenUI 正确显示
- 改父 / 改 Anchor·Margin·Preset 后重算 Computed Rect，画面更新
- `WidgetComponent` 不承载 Texture/Color（由 Image 提供）

---

## 2) 现状（代码事实，2026-09-05）

| 能力 | 状态 |
|------|------|
| ScreenUI Pass / Queue | **有**（RND-F16 Path B） |
| `WidgetComponent` | **有** → 本 Feature 收窄为 Layout 基础；Texture/Color **迁到 Image** |
| `GameObject` 父子 | **有**（CORE-F08/F09） |
| Hierarchy 树 + 拖拽改父 | **有**（ED-F05） |
| Layout / Canvas / Image | **本 Feature 新增** |

---

## 3) 方案

### 3.1 组件分层（拍板）

```text
GameObject (Canvas root)
 └── CanvasComponent
      └── child GOs…（GO 父子）
           ├── WidgetComponent   ← Layout + StableOrder + ComputedRect
           └── ImageComponent    ← Texture + Tint
```

| 类型 | 职责 | 非职责 |
|------|------|--------|
| **`WidgetComponent`** | Anchor/Margin/Size、Preset、ComputedRect、StableOrder | 贴图/字体样式 |
| **`ImageComponent`** | Texture + Color | Layout |
| **`CanvasComponent`** | 参考分辨率、Letterbox、子树根 | Hit / 样式 |

> **不另拆 `LayoutComponent`。**  
> **一次切干净：** Widget 删除 Texture/Color；旧场景需补 Image。

### 3.2 Canvas（ScreenSpace）

| 项 | MVP 约定 |
|----|----------|
| `RenderMode` | 仅 **ScreenSpace**（World 预留枚举可不实现） |
| 参考分辨率 | 默认 `1920×1080`（可配置） |
| **Scale Mode** | **Letterbox**（等比适配，居中；可能留边） |
| 子节点坐标 | Layout 在 **Canvas 参考像素空间**；`BuildScreenUIQueue` 映射到视口像素 |
| 谁入队 | Canvas 子树内、有效 Widget + Image 的节点 |

```text
参考像素 ComputedRect
        ↓  Letterbox（scale = min(vp/ref), 居中 offset）
视口像素 → ScreenUIPass
```

### 3.3 Layout 基本规则

**Tree ≠ Layout。** GO 父子提供树；Layout 产出 Computed Rect。

| 概念 | 含义 |
|------|------|
| `Size` | 点锚定时的固定宽高（参考像素） |
| `AnchorMin` / `AnchorMax` | 相对父 Computed Rect 的归一化角点 |
| `Margin` | `(L,T,R,B)`：相对锚点矩形内缩；点锚定时 L/T 为自锚点偏移 |

**点锚定**（Min≈Max）：`TopLeft = anchor + (L,T)`，`Size` 用字段 Size。  
**拉伸**（Min≠Max）：`Rect = anchorRect inset by Margin`；忽略 Size（宽高由锚点框决定）。

**计算顺序：** Canvas.RefRect = `(0,0, RefW, RefH)`；子树 DFS，父先于子。无 Widget 的中间 GO 向下传递同一 parentRect。

**绘制：** 使用 ComputedRect，**不**用 `GetWorldTransform()`（避免与 3D/KeepWorld 混用）。

### 3.4 Anchor Preset（编辑体验）

底层始终是 `AnchorMin`/`AnchorMax`（+ Margin）。  
产品层：`EUIAnchorPreset` → `ApplyAnchorPreset()` 写入 Min/Max（Margin 清零）；与 Unity/UE 同构。

MVP 预设至少覆盖：九宫格点锚定 + StretchAll + 常见边拉伸（顶/底水平、左/右垂直）。  
Inspector：改 Preset 即 Apply；亦可直接改 Min/Max（高级）。

### 3.5 ImageComponent

| 属性 | 说明 |
|------|------|
| `Texture` | 可选；空则白贴图 × Tint |
| `Color` | RGBA Tint |

**Invariant：** 无 Widget 或无 Image → 不入队。

### 3.6 数据流

```text
[Tick] Canvas → UILayoutPass（参考空间 ComputedRect）
[EOF]  Widget dirty → FillSceneProxy（ref-space TopLeft/Size + Image 样式）
[Render] BuildScreenUIQueue：Letterbox 映射 → ScreenUIPass
```

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| 另建 Widget Tree Runtime | **拒绝** |
| 仅用 SceneComponent 附着模拟 UI 树 | **拒绝** |
| Layout 直接写 GPU | **拒绝** |
| Stretch 与 Letterbox 同期 | **Defer Stretch**；MVP 只 Letterbox |
| 枚举替代 AnchorMin/Max | **拒绝** — Preset 只是写入器 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| Widget 去 Texture 破坏旧场景 | 一次迁移；补 Image；CHANGELOG/Progress 说明 |
| Layout 每帧全树重算 | MVP 可接受；dirty 后置 |
| GO Transform 与 UI 坐标冲突 | 绘制只用 ComputedRect |

---

## 6) 验收标准（MVP）

- [x] `CanvasComponent` ScreenSpace + 参考分辨率 + Letterbox
- [x] Widget Anchor+Margin+Size → Computed Rect；Preset Apply
- [x] `ImageComponent` + 收窄后的 Widget 可画
- [x] Canvas 子树随 GO 父子；改父后 Layout/绘制正确（单测；Editor 目视待确认）
- [x] 仍走 ScreenUI；无 Text/Button/Flex/Hit-test

---

## 7) 实现范围（一次验收）

| 内容 | 说明 |
|------|------|
| Canvas + Letterbox 映射 | `ScreenUICoords` + `CanvasComponent` |
| Widget Layout + Preset | 字段 + `UILayoutPass` |
| Image + Widget 去样式 | forward-only 删除 Texture/Color |
| Queue 过滤 | 仅 Canvas 子树 + Widget+Image |
| 单测 | Letterbox 映射 + Layout 点锚定/拉伸 |

---

## 8) 前置（已满足）

CORE-F08 / CORE-F09 / ED-F05 / RND-F16 Path B 均 **Done**。本 Feature 不再 blocked。

---

## 9) Status note

| 字段 | 内容 |
|------|------|
| Status | **In Progress**（代码+单测；**未目视验收**） |
| Blocked by | — |
| Next | 合入 core 分支 Setter/Getter 后目视（Preset 直改字段当前无效）→ Done |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 占位 |
| 2026-09-03 | Canvas GO 方向；Widget 命名；对齐 RND-F16 |
| 2026-09-04 | Draft：Canvas + Layout + Image；曾阻塞 CORE-F08 |
| 2026-09-04 | Layout 并入 Widget；Inactive 传播不属本 Feature |
| 2026-09-05 | **Planned→In Progress：** 前置已齐；拍板 Letterbox；Anchor Preset；一次落地 MVP |
| 2026-09-05 | **MVP 代码落地：** Canvas/Letterbox、Widget Layout+Preset、Image；Widget 去 Texture/Color；screen-ui-coords/ui-layout PASS；**未目视验收** |
| 2026-09-05 | Status 保持未验收：Inspector 直写 Preset 不触发 Apply；等 core Setter/Getter 合入后再目视 |
