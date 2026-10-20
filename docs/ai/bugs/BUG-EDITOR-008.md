# BUG-EDITOR-008 — 把 Texture2D 写进 ImageComponent.m_Sprite 会让编辑器访问违例

## Meta
- **ID:** BUG-EDITOR-008
- **Status:** Fixed（2026-10-20；引擎侧反序列化守卫 + 编辑器侧 codec 前置校验，双守卫各自可反向验证）
- **Severity:** S2（自动化 misuse 即崩，无类型校验）
- **Owner:** 待分配（Image/Sprite 解析路径）
- **Found:** 2026-10-19
- **Affects:** `ImageComponent` 解析 `m_Sprite` + 绘制（`ScreenUIMaterialFactory` / Sprite 解析）
- **Related Feature/Slice:** [MCP-F05](../Editor/MCP/MCP-F05_PREFAB_AUTHORING_AND_PROPERTY_REFS_DESIGN.md)（`set_property` 引用解析让误绑变得容易触发）；minCraft `GP-F02` S1g

## TL;DR

通过自动化把 `Textures/**/*.metex`（Texture2D）写入 `ImageComponent.m_Sprite` 后，编辑器在绘制该 widget 时以 `0xC0000005` 崩溃。`m_Sprite` 期望的是 `Sprite` 资产，但写入与绘制两侧都没有类型校验。

**修复（2026-10-20）：** 根因不是「绘制侧缺防御」，而是**反序列化的资产引用分支缺类型校验**：`Serializer::ResolvePendingAssetRef` 拿到资产就绑，而同文件 MEObject 分支是有 `IsClassSameOrDerived` 校验的（一处有、一处没有的不对称）。类型混淆的 `shared_ptr` 一旦生成，绘制侧无法自证，**唯一有效防线在绑定那一刻**。

两道守卫（都可单独反向验证）：
1. **引擎侧**：`ResolvePendingAssetRef` 增加 `expectedClass` vs 资产 `GetClass()` 的 `IsClassSameOrDerived` 校验，不符返回 `type mismatch: expected <A>, got <B>`；并把首条未解析原因带进 `ResolvePendingObjectRefs` 的聚合失败消息（原本只进日志，调用方分不清「GUID 不存在」与「类型不符」）。
2. **编辑器侧**：`ScenePropertyValueCodec::TryWriteJsonToLive` 在写入前逐条比对，不符返回既有的 `RefError::Invalid` → 映射为 `InvalidPayload`，**零协议变更**（`ERROR_CODES.md` / `response.schema.json` 未动）。

回归：`asset-ref-type`（2 例，含「派生类绑定不被误杀」）与 `scene-property-ref-type`（2 例）进入 `test smoke` 门禁；摘任一守卫用例即变红，两者全摘则写入被接受（复现修复前的崩溃/静默写坏状态）。

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
