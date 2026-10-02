# BUG-ASSET-002 — AssetSerializeHelper 用 MEObject::StaticClass 导致 Scene 冷加载丢 GO

## Meta
- **ID:** BUG-ASSET-002
- **Status:** Fixed
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-10-02
- **Last updated:** 2026-10-02
- **Affects:** `AssetSerializeHelper` → Scene / Prefab / Material 等经 Helper 的 Load/Save；消费侧 minCraft 冷开场景 Hierarchy 空
- **Related Feature/Slice:** ASSET-F06（`AssetSerializeHelper`）；minCraft 反馈 `ENGINE_FEEDBACK_PROMPT_SCENE_SERIALIZE_STATICCLASS.md`

## TL;DR
`ReadObjectFromFile` / `WriteObjectToFile` 以 `MEObject&` 调用模板 `Serializer::FromFile`/`ToFile`，`T` 推导为 `MEObject`，只用基类字段；Scene 的 `m_GameObjects` 被 skip → 冷加载空场景，Save 写空盘。

---

## 症状
1. 磁盘 `.mescene` 含 GO，冷开后 Hierarchy 0 roots。
2. 日志：`unknown field 'm_GameObjects' skipped`（及 PrefabInstances / SceneName）。
3. `scene.save` 可能把磁盘写成仅 MEObject 字段（`m_GameObjects: []` 或缺失派生字段）。

## 期望
- Load/Save 经 Helper 时使用 **运行时** `object.GetClass()`。
- Scene 往返后 GO 计数与磁盘字段保留。

## 复现
1. minCraft：冷开含 Host 的 `Scenes/default.mescene` → Hierarchy 空。
2. 或单测：`prefab scene AssetSerializeHelper keeps game objects [smoke]`（修复前失败）。

## 环境
- 引入：ASSET-F06 facade（`1268f819`）
- Windows Debug Editor / minEngineTests

## 根因
模板 `Serializer::FromFile<T>` / `ToFile<T>` 使用 `T::StaticClass()`。Helper 签名为 `MEObject&` → `T = MEObject`，不是 `Scene::StaticClass()` / `GetClass()`。

## 修复
- `AssetSerializeHelper`：`Serializer::FromFile/ToFile(path, object.GetClass(), &object, …)`。
- 回归：`PrefabTest` 中 Scene Write→Read→Rewrite，断言文件含 `m_GameObjects` / 根名且 Load 后 GO 数 = 1。

## 回归验证
- [x] `minEngineTests.exe test prefab`（含 `AssetSerializeHelper keeps game objects`）
- [ ] minCraft 冷开 `default.mescene` Hierarchy 非空（消费侧手验）

## 关联
- minCraft：`docs/ai/ASSET/ENGINE_FEEDBACK_PROMPT_SCENE_SERIALIZE_STATICCLASS.md`
- ASSET-F06 Design

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-02 | Fixed：Helper 改用 GetClass；加回归测 |
