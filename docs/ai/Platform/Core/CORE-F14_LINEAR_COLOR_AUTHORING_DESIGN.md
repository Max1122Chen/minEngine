# CORE-F14 — LinearColor Authoring Colors

## Meta
- **ID:** `CORE-F14`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Branch:** `feat/ui`
- **Related:**
  - [UI-F01](../UI/UI-F01_UI_SYSTEM_DESIGN.md)（Image tint；已 Done）
  - [Color.h](../../../../minEngine/minEngine/src/Runtime/Core/Math/Color.h)（`LinearColor` / `Color`）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** CORE-F11 AssignProperty/Setter（已合入）；UI-F01 Done
- **Blocks:** —
- **Implementation:** 一次落地（无旧盘兼容）；可不另建 Impl 文件

## TL;DR

把**作者侧颜色属性**从 `Vector4` 统一为 **`LinearColor`**（Image / Sprite / Light），Inspector 走 `ColorWidget`（含 **显式 Close**）；仓库场景 forward-only 改写为 struct JSON。**不**兼容旧 `[r,g,b,a]` 读盘。

## Scope

### In（改动清单）

| 区域 | 文件 / 符号 |
|------|-------------|
| Math | `LinearColor::ToVector3/ToVector4`（渲染边界） |
| Components | `ImageComponent` / `SpriteComponent` / `LightComponent` 色字段 → `LinearColor` + Setter |
| Call sites | Widget sync、Sprite factory、Light Proxy 赋值、PreviewScene |
| Translucency | `ComputeSpriteNeedsTranslucentPass` 接受 `LinearColor` |
| Editor | `ColorWidget`：ColorButton+Popup+Picker+**Close**；删除 Vector4 ColorEdit 启发式 |
| Assets | 仓库 `.mescene` 中 `m_Color` / `m_LightColor` 改为 `{R,G,B,A}` |

### Out
- `WidgetComponent::m_Margin`、`UVRect` 等非颜色 `Vector4`
- Proxy / GPU UBO 内部改成 `LinearColor`
- 材质图 Constant；批量 `.memtl`
- **旧 Vector4 数组读盘兼容**（forward-only；开发期重存场景即可）
- 8-bit `Color` 作为作者存储

## Reader quick start
1. §3 类型边界
2. §3.2 ColorPicker Close
3. §6 验收

---

## 1) 背景与目标

**Pain：** Tint/灯光色用 `Vector4`，与主题 `LinearColor` 不一致；临时名字启发式 ColorEdit。

**成功标准：** 三组件 Inspector 用 `ColorWidget`；alpha/tint 仍生效；Picker 有 Close；场景为 struct；无旧数组兼容代码。

---

## 2) 现状

| 项 | 状态 |
|----|------|
| `LinearColor` + `ColorWidget` | 有（主题） |
| Image / Sprite / Light | 曾为 `Vector4` |
| UI-F01 启发式 | Vector4 ColorEdit（本 Feat 删除） |

---

## 3) 方案

### 3.1 类型契约

| 层 | 类型 |
|----|------|
| 组件 / 序列化 / Inspector | `LinearColor` |
| Proxy / Material / UBO | `Vector3` / `Vector4`（`ToVector3` / `ToVector4`） |

灯光：色度 `LinearColor`（RGB；A=1）；强度 `m_Intensity`。

### 3.2 序列化（forward-only）

- 写：`{ "R","G","B","A" }`
- **不**读旧数组；仓库场景一并改写
- 用户本地旧场景：Editor 中重设色或手改 JSON

### 3.3 ColorPicker 显式关闭

`ColorWidget`：`ColorButton` → `BeginPopup` → `ColorPicker4`（Alpha）→ `Close`（`CloseCurrentPopup`）。

### 3.4 落地

单次落地：代码 + 仓库场景 + 删启发式；无需 S01 兼容层。

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| 旧数组读兼容 | **拒绝**（用户确认 forward-only） |
| 存 8-bit `Color` | **拒绝** |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 旧本地场景色字段加载失败 | 文档说明；重存 / 手改 |
| Light 旧 A 滥用 | 迁移时 A=1，强度用 Intensity |

---

## 6) 验收标准

- [x] Image / Sprite / Light 为 `LinearColor` + Color 控件
- [x] tint × texture；alpha 生效
- [x] ColorPicker 有显式 Close
- [x] 仓库场景已为 struct；**无**旧数组读路径
- [x] Vector4 ColorEdit 启发式已删
- [x] 构建 + `ui-layout` / `screen-ui-coords` / `sprite-translucency` PASS

---

## 7) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done**（代码 + 单测 + 目视） |
| Blocked by | — |
| Next | — |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | Planned：统一 LinearColor + ColorPicker Close |
| 2026-09-05 | **In Progress：** 去掉旧盘兼容；明确改动清单；开始实现 |
| 2026-09-06 | 代码落地；场景改写；测试 PASS；Status → Review |
| 2026-09-06 | **Done：** 目视通过（ColorPicker Close + 三组件改色） |
