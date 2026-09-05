# CORE-F14 — LinearColor Authoring Colors

## Meta
- **ID:** `CORE-F14`
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Branch:** `feat/ui`
- **Related:**
  - [UI-F01](../UI/UI-F01_UI_SYSTEM_DESIGN.md)（Image tint；已 Done）
  - [Color.h](../../../../minEngine/minEngine/src/Runtime/Core/Math/Color.h)（`LinearColor` / `Color`）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** CORE-F11 AssignProperty/Setter（已合入）；UI-F01 Done
- **Blocks:** —
- **Implementation:** 本 Feature 内 1–2 切片；可不另建 Impl 文件

## TL;DR

把**作者侧颜色属性**从 `Vector4` 统一为 **`LinearColor`**（Image / Sprite / Light），Inspector 走已有 `ColorWidget`；场景 JSON 迁移（含旧数组读兼容）；并给 ColorPicker 弹层加**显式关闭**按钮。

## Scope

### In
- `ImageComponent` / `SpriteComponent` / `LightComponent`：存储与反射 → `LinearColor`
- 渲染边界：`LinearColor` → `Vector3`/`Vector4` 再进 Proxy / `ApplyColorAndTexture`
- 仓库 `.mescene` 对应字段改写；读盘兼容旧 `[r,g,b,a]` 数组
- `ColorWidget`：Picker 弹出后提供显式 **Close**（不仅依赖点空白关闭）
- 删除 `PropertyPrimitiveWidgets` 对 `*Color*`/`*Tint*` `Vector4` 的 ColorEdit 启发式（改回真正的 `LinearColor` 路径）

### Out
- `WidgetComponent::m_Margin`、`UVRect` 等非颜色 `Vector4`
- Proxy / GPU UBO 内部改成 `LinearColor`
- 材质图 Constant 节点类型改造；批量改 `.memtl`（BaseColor 贴图名无关）
- 8-bit `Color` 作为作者存储（仅 display / 转换）

## Reader quick start
1. §3 类型边界与迁移
2. §3.3 ColorPicker Close
3. §6 验收

---

## 1) 背景与目标

**Pain：** Tint/灯光色用 `Vector4`，Inspector 体验分裂；临时按属性名猜 ColorEdit；与主题已用的 `LinearColor` 不一致。

**成功标准：**
- 三件套组件 Inspector 用 `ColorWidget`（sRGB 显示 ↔ linear 存储）
- 改色 / alpha 同步材质（Setter 仍走）
- 旧场景数组格式可读；新存盘为 struct
- ColorPicker 有显式关闭控件

---

## 2) 现状

| 项 | 状态 |
|----|------|
| `LinearColor` / `Color` + `ColorWidget` | 有（主题已用） |
| Image / Sprite / Light 色 | `Vector4` JSON 数组 |
| UI-F01 临时启发式 | `PropertyPrimitiveWidgets` 对 Color 名 `Vector4` 用 ColorEdit4 |
| Light Proxy | `Vector3`；Intensity 独立字段 |

---

## 3) 方案

### 3.1 类型契约

| 层 | 类型 |
|----|------|
| 组件 / 序列化 / Inspector | `LinearColor` |
| Proxy / Material factory / UBO 打包 | `Vector3` / `Vector4`（边界转换） |
| 8-bit `Color` | 仅 sRGB 交换 / 显示转换 |

灯光：色度 `LinearColor`（RGB；A 默认 1）；强度仍 `m_Intensity`。勿把 HDR 强度塞进 A。

### 3.2 序列化迁移

- **写：** 反射 struct → `{ "R","G","B","A" }`
- **读兼容：** 若字段仍是 JSON 数组 length≥3，灌入 `LinearColor`（缺 A 则 1）
- 仓库 `MyMEProject` 场景手改或 Editor 重存

### 3.3 ColorPicker 显式关闭

现状：`ImGui::ColorEdit4` 弹出 picker 后主要靠点空白关闭，无显式按钮。

**方案（推荐）：** 在 `ColorWidget` 内用 `ColorButton` + `BeginPopup` + `ColorPicker4`，popup 底部加 `Close`（`CloseCurrentPopup`）；保留 Alpha。避免依赖 ImGui 内部 picker chrome 版本差异。

### 3.4 切片

| Slice | 内容 |
|-------|------|
| **S00** | 三组件 → `LinearColor`；边界转换；去 Vector4 启发式；ColorWidget Close |
| **S01** | 读兼容 + 仓库场景迁移；冒烟打开旧/新场景 |

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| 继续 `Vector4` + 名字启发式 | **拒绝** |
| 存 8-bit `Color` | **拒绝**（与渲染/主题线性语义不符） |
| Proxy 也改 `LinearColor` | **Defer**（收益低） |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 旧场景打不开 | 数组读兼容 + 仓库资产改写 |
| Light A 曾被滥用 | 迁移时忽略异常 A；用 Intensity |
| ColorPopup 交互回归 | 目视：开/关/改色/alpha |

---

## 6) 验收标准

- [ ] Image / Sprite / Light 属性类型为 `LinearColor`，Inspector 为 Color 控件
- [ ] Image/Sprite tint × texture；alpha 仍生效
- [ ] ColorPicker 有显式 Close，关闭后 popup 消失
- [ ] 旧数组场景可读；新存盘为 struct
- [ ] Vector4 ColorEdit 启发式已删除
- [ ] `ui-layout` / 相关冒烟 + 目视

---

## 7) Status note

| 字段 | 内容 |
|------|------|
| Status | **Planned** |
| Blocked by | — |
| Next | S00 实现 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | Planned：统一 LinearColor + 场景迁移 + ColorPicker Close |
