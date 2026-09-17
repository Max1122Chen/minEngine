# BUG-CORE-002 — Prefab Stage 根 Transform / Create 世界 Scale / Stage Save 失败

## Meta
- **ID:** BUG-CORE-002
- **Status:** Fixed
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-09-16
- **Last updated:** 2026-09-16
- **Affects:** `PrefabUtility::Instantiate` / `WriteStageTreeToPrefab`；`PrefabStageController`；Create Prefab → Stage → Propagate 手验链；`feat/prefab`
- **Related Feature/Slice:** CORE-F23 / CORE-F24 / ED-F16 · Fix Design：[BUG-CORE-002 Fix Design](../Platform/Core/BUG-CORE-002_PREFAB_STAGE_TRANSFORM_SAVE_FIX_DESIGN.md)

## TL;DR
Prefab Stage **强制把根刷成 identity**、Create 对世界 Scale 不稳、Stage **二次 Save 因 EditCloneMap 丢根映射失败**——合为一条；挡住 default 传播手验。删资产不断链 → **另开 CORE-F26**，不进本 bug。

---

## 症状（合并原 1–4）

1. **根 Transform「传播」体感：** 开 Prefab Stage 后根姿态变「初态」；Save 后 Level 实例姿态异常（或看起来被刷回初态）。
2. **Create 世界 Scale：** 从带父级缩放的源树 Create Prefab 时，模板/后续实例未稳定保留源根 **世界 Scale**。
3. **Stage 无法保存：** 日志  
   `Save Prefab Stage: EditCloneMap missing Prefab root mapping; cannot Save without Guid table.`  
   同会话内可出现「先成功一次、watcher 全扫后再 Save 失败」。
4. **传播验不了：** #3 挡住改 CastShadows 等 default → Save → 源树更新的手验（Guid 身份已由 ASSET-F03 对齐）。

## 期望

- Stage 打开后根 Transform = Prefab 模板根（局部=世界，因无父）姿态，**不得**无条件写成 identity。
- Create Prefab：模板根应反映源根 **世界** 姿态（含 Scale）；Level 源实例姿态不被 Create 本身改写。
- Stage Save：同会话可重复 Save；`EditCloneMap` 与 `Prefab::GetRootGuid()` 持续对齐。
- 根 Transform **不**经 Propagate 改写 Level 摆放（F24 O1）；非根 default 在 Save 后可传播。

## 复现（摘要）

1. Level：带非单位 Scale / 非原点 Transform 的 GO → Create Prefab → 观察模板与源实例。
2. 双击 `.meprefab` 进 Stage → 观察根是否变 identity。
3. Stage Save → 再 Save（或等 watcher 全扫后）→ 是否出现 EditCloneMap warning。
4. （若 Save 通）改非 Transform default → Save → Level 实例是否更新；确认根 Transform 未 Propagate。

## 环境

- OS：Windows；`feat/prefab`；OpenGL Editor
- 日志样本：`MaximumEditor` Console（2026-09-16）含上述 EditCloneMap 文案与 `DeleteAsset: reference scan is not implemented`（后者属 CORE-F26，非本修）

## 根因（初步，Fix Design 展开）

| # | 初步根因 |
|---|----------|
| A | `Instantiate` 末尾 `SetWorldTransform(params.WorldTransform)`；默认 `Transform{}` = identity；**BuildStage 未传入模板根姿态** |
| B | Create 克隆局部 Transform；父链 Scale 依赖 `Detach(KeepWorld)` bake；与 A 叠加后 Stage/再实例易丢世界 Scale |
| C | `WriteStageTreeToPrefab` 依赖 `EditCloneMap[RootGuid]`；回写后 map 刷新与资产/watcher 时序存在失配窗口（待修时钉死） |
| D | 非独立根因：被 C 阻塞 |

## 修复

见 [Fix Design](../Platform/Core/BUG-CORE-002_PREFAB_STAGE_TRANSFORM_SAVE_FIX_DESIGN.md)（**Review，待审批**）。

## 回归验证

- [x] Stage 打开：根 Transform = 模板根；非强制 identity（`bApplyWorldTransform` + 单测）
- [x] Create：源根世界 Scale 进入模板根；源 Level 实例姿态不变（含外部父 Create 清 unresolved）
- [x] Stage 连续 Save ≥2：EditCloneMap 根映射保持（Scene 查找 + 并行树 rebuild；单测模拟 Unregister）
- [x] 根 Transform 不 Propagate（既有 `prefab-overrides`）；非根 default 传播既有覆盖
- [x] `test prefab` / `test prefab-overrides` 绿

## 修复摘要（2026-09-16）

1. **`PrefabInstantiateParams::bApplyWorldTransform`**（默认 false）；Hierarchy Instantiate 显式 true+identity。
2. **Create** 捕获源世界 Transform 并 bake；外部父/attach unresolved 清空后继续（不再因 resolve 失败整单失败）。
3. **`RemapObjectGuid` / Clone 根**：仅当 ObjectManager 槽仍指向本对象才 Unregister（修 Stage Save 丢根映射根因）。
4. **WriteStageTree**：Scene 查找刷新 map；缺根则并行树 rebuild；失败回滚模板列表。

## 关联

- [ASSET-F03](../Asset/ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)（Guid 已修，不重复）
- [BUG-ASSET-001](./BUG-ASSET-001.md)（全盘 Scan；可加剧 C 的时序，但不替代本修）
- [CORE-F26](../Platform/Core/CORE-F26_PREFAB_ASSET_DELETE_UNLINK_DESIGN.md)（删资产断链 — **Out**）
- [BUG-CORE-003](./BUG-CORE-003.md)（传播只拷 Name/Transform — 手验「改 default 不跟」的真因）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-16 | **Fixed：** S01–S04 实现；`test prefab` 12/12、`prefab-overrides` 3/3 PASS |
| 2026-09-16 | Note：手验仍见 default 不传播 → 另登 **BUG-CORE-003**（非本修范围） |
| 2026-09-16 | Open：合并 Stage Transform / Create Scale / Save map / 传播手验阻塞；Fix Design → Review |
