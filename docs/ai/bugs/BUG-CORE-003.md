# BUG-CORE-003 — Prefab default 传播未扫反射属性

## Meta
- **ID:** BUG-CORE-003
- **Status:** Fixed
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-09-16
- **Last updated:** 2026-09-16
- **Affects:** `PrefabOverrideUtility::PropagateDefaultsToScene`；Prefab Stage Save 后 Level 实例；`feat/prefab`
- **Related Feature/Slice:** CORE-F24 §3.4.3 · Fix Design：[BUG-CORE-003 Fix Design](../Platform/Core/BUG-CORE-003_PREFAB_PROPAGATE_REFLECTED_PROPERTIES_FIX_DESIGN.md)

## TL;DR
F24 设计要求对模板 **每个反射属性** 传播；实现只拷 **`m_Name` + 非根 `m_Transform`**。另：**Editor 未调用** `TryRecordPropertyOverride`（S02 半截）。新 Instantiate 能带上改过的 default（整树克隆），已有实例在 Prefab Save 后不更新。Guid 身份不是本条根因。

**Fix 包（Fixed）：** 全属性 Propagate + Editor 记 Override + ValidateEdit — 见 [Fix Design](../Platform/Core/BUG-CORE-003_PREFAB_PROPAGATE_REFLECTED_PROPERTIES_FIX_DESIGN.md)。

---

## 症状

1. Prefab Stage 改非 Transform default（如 `CastShadows`）→ Save → Level 里已有实例不变。
2. 同一会话再 **Instantiate** 一份 → 新实例带当前模板值。
3. 根 Transform 按设计不传播（O1）；与本缺口正交。

## 期望

Prefab Save 后 `PropagateDefaultsToOpenScenes` 对开着的 Editor Scene：无 override 的对应字段等于模板；根 Transform 仍跳过。

## 复现

1. Level Create Prefab 或 Instantiate，确认 `PrefabInstanceRecord.PrefabAssetGuid == Prefab.GetGuid()`。
2. 打开 Prefab Stage，改根以外组件的非 Transform 字段，Save。
3. 切回 Level：旧实例应更新；实际不更新。再 Instantiate：新实例已是新 default。

## 环境

- OS：Windows；`feat/prefab`；OpenGL Editor
- 代码：`PrefabOverrideUtility.cpp` `PropagateDefaultsToScene` / `CopyNonRootPropertiesFromTemplate`

## 根因（初步）

`PropagateDefaultsToScene` 未实现 F24「For each reflected property」。仅：

- GameObject：`m_Name`
- `SceneComponent`：`m_Transform`（根实例 Root 再跳过）

组件其余 `ME_PROPERTY` 从不 `SerializePathToPayload` / `ApplyPayloadToPath`。`PropagateDefaultsToOpenScenes` 会调到 `GetEditorScene()`（与 Document Scene 同指针），**挂钩在**；缺的是属性遍历。

次要静默跳过（修全量传播时一并断言，不单开 bug）：`PrefabAssetGuid` 失配；`FindObject(instanceGuid)` 失败。

## 修复

见 Fix Design（**Done / Fixed**）。范围含：

1. 全反射属性 Propagate（原 003）
2. F24 S02：Editor → `TryRecordPropertyOverride`
3. F24 S05：破坏性编辑 → `ValidateEdit`
4. 测试 / DoD 收口  

**不**含：Added/RemovedComponent 行为、Apply→Prefab、Instantiate 偏移、磁盘全项目传播。

## 回归验证

- [x] Stage 改 CastShadows（或等价）→ Save → Level 旧实例更新（单测 `propagate non-transform`）
- [x] Level 改实例属性 → Overrides（`ApplySetObjectProperty` → TryRecord；手验）
- [x] 有 PropertyValue override 的字段不覆盖；根 Transform 仍不传播（既有 + 新测）
- [x] 删实例根被拒（ValidateEdit + Level PrefabEditConstraints）
- [x] `test prefab-overrides` 5/5；`test prefab` 14/14 绿

## 修复摘要（2026-09-16）

1. **Propagate / RevertInstance：** 按 mapping 扫反射叶子（跳过 Instanced/Delegate/非资产 ObjectPtr/根 Transform/override）。
2. **Editor：** `ApplySetObjectProperty` → `TryRecordPropertyOverride`；Stage Save 传播成功则 `MarkDocumentSceneDirty`。
3. **ValidateEdit：** Level 删/重挂走 `PrefabEditConstraints` → `PrefabOverrideUtility::ValidateEdit`。

## 关联

- [CORE-F24](../Platform/Core/CORE-F24_PREFAB_OVERRIDES_DESIGN.md) Amendment A/B
- [BUG-CORE-002](./BUG-CORE-002.md)（Stage 姿态/Save map — **Fixed**）
- [ASSET-F03](../Asset/ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)（Guid — **Done**）
- [CORE-F26](../Platform/Core/CORE-F26_PREFAB_ASSET_DELETE_UNLINK_DESIGN.md)（删资产断链 — Out）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-16 | Open：对照 F24 §3.4.3 与实现；Instantiate 偏移后置 |
| 2026-09-16 | Fix 包扩 F24 S02/S05 收口；用户同意 Propagate 方案，整包待再批 |
| 2026-09-16 | **Fixed：** 全属性 Propagate + TryRecord 挂钩 + ValidateEdit；`prefab-overrides` 5/5 |
