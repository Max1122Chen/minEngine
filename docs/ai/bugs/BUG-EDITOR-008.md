# BUG-EDITOR-008 — 把 Texture2D 写进 ImageComponent.m_Sprite 会让编辑器访问违例

## Meta
- **ID:** BUG-EDITOR-008
- **Status:** Open
- **Severity:** S2（自动化 misuse 即崩，无类型校验）
- **Owner:** 待分配（Image/Sprite 解析路径）
- **Found:** 2026-10-19
- **Affects:** `ImageComponent` 解析 `m_Sprite` + 绘制（`ScreenUIMaterialFactory` / Sprite 解析）
- **Related Feature/Slice:** [MCP-F05](../Editor/MCP/MCP-F05_PREFAB_AUTHORING_AND_PROPERTY_REFS_DESIGN.md)（`set_property` 引用解析让误绑变得容易触发）；minCraft `GP-F02` S1g

## TL;DR

通过自动化把 `Textures/**/*.metex`（Texture2D）写入 `ImageComponent.m_Sprite` 后，编辑器在绘制该 widget 时以 `0xC0000005` 崩溃。`m_Sprite` 期望的是 `Sprite` 资产，但写入与绘制两侧都没有类型校验。

## 复现

1. 场景内建 `Canvas + Widget + Image`；
2. `scene.set_property`：`{ "propertyPath": "m_Sprite", "value": { "$assetRef": "Textures/Gui/inventory.metex" } }`（`$assetRef` 解析成功、写入成功）；
3. 视口绘制该 widget → AV，`ed_crash.log`。

## 期望

- `set_property` 写入时做**类型校验**：目标属性类型为 `Sprite*` 时拒绝 `Texture2D`，返回可诊断错误（例如 `TypeMismatch` 带期望/实际类型）；
- 绘制侧对 `m_Sprite` 的解析加防御（类型不符 → 走 `CanDraw=false` 分支，而不是崩）。

## 绕路（已采用）

消费方改用 `ImageComponent.SourceMode = Raw` + `m_Texture`（整图 1:1 使用，语义等价）；仅子矩形精灵走 `sprite.create_from_texture` + `sprite.save` 产出 `.mesprite` 再绑 `m_Sprite`。

## 退出条件

`set_property` 对 `m_Sprite` 误绑返回 `TypeMismatch` 且编辑器不崩；补一条自动化断言（写入拒绝 + 场景可继续运行）。
