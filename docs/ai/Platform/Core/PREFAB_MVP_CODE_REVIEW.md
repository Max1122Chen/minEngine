# Prefab MVP（feat/prefab）— 工程 Code Review 报告

## Meta
- **Type:** Code Review（合入前工程复盘）
- **Status:** Done（全包已落地；2026-09-17）
- **Owner:** project maintainer
- **Last updated:** 2026-09-17
- **Branch:** `feat/prefab`
- **Related:**
  - [CORE-F23](./CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md) · [CORE-F24](./CORE-F24_PREFAB_OVERRIDES_DESIGN.md) · [CORE-F25](./CORE-F25_OWNING_SCENE_RENDERSCENE_DESIGN.md) · [CORE-F26](./CORE-F26_PREFAB_ASSET_DELETE_UNLINK_DESIGN.md)
  - [ED-F16](../../Editor/ED-F16_PREFAB_EDITOR_DESIGN.md) · [ASSET-F03](../../Asset/ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)
  - [BUG-ASSET-001](../../bugs/BUG-ASSET-001.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Diff scope (review baseline):** `master...feat/prefab` ≈ 95 files，+9912 / −234  
- **Verified after fix pack:** `test prefab` 16/16；`test prefab-overrides` 5/5（2026-09-17）

---

## TL;DR

Prefab MVP **功能可工作且已手验**。全包修复/精简已于 2026-09-17 落地（A1–A5 + B1–B5），见 §7 落地记录。

历史分类与方案仍保留于下文，供合入说明对照。

---

## 0) 分类原则

| | 功能缺口（A） | 冗余 / 结构债（B） |
|--|---------------|-------------------|
| 判据 | 错误路径、缺门禁、契约与实现不一致、可导致错误数据 | 行为正确但重复/难维护/多余 I/O |
| 不放这里 | 已明确 **Out** 且文档诚实的能力（Nested、Apply→Prefab、Inspector 蓝字、并排 RT） | 「再做一轮大重构」而无目标结构 |
| Out 项 | 若实现**半套**（枚举有、门禁无）→ 算缺口，不算「以后再说」 | — |

---

## 1) 功能缺口（A）

### A1 — Level Prefab 实例：组件 Add/Remove 无门禁、无 override 记账

| 项 | 内容 |
|----|------|
| **严重度** | P2 |
| **现象** | Hierarchy/Inspector 可对 Prefab 实例增删非 Root 组件；`ObjectMappings` 留下死 Guid；Propagate 只 Warn 跳过 |
| **证据** | `SceneEditor::ApplyRemoveComponentFromGO` / Add 路径**不**调用 `PrefabEditConstraints` / `ValidateEdit`；删 GO 路径**会**调用。`PrefabEditValidator::RemoveComponent` 规则存在但 Editor 未接线 |
| **与设计关系** | F24 将 Added/RemovedComponent 标 **Out**；同 Feature 已禁止删 mapped 子节点。组件放行 = **策略不一致**（半套） |
| **触发** | Level 选中实例 → Remove Mesh/Light 等；或 Add 新组件后存盘 |
| **影响** | 实例相对资产永久漂移；Scene 内假 mapping；删 Prefab / Unpack 语义仍按 Record，与真实树不一致 |

#### 方案 A1-α（推荐 · MVP 收口）

- 在 `ApplyAddComponent*` / `ApplyRemoveComponent*`（及 Command 入口）接入 `PrefabEditConstraints`。
- 若目标 GO（或组件 Guid）落在某 `PrefabInstanceRecord.ObjectMappings` 内：
  - **拒绝** Remove（含非 Root）；**拒绝** Add（mapped 树上加组件）。
- 错误文案与删子节点一致：「CORE-F24 MVP 不支持…」。
- 单测：实例上 Remove/Add 失败；普通 GO 不受影响。

#### 方案 A1-β（不推荐本期）

- 实现 `EPrefabOverrideKind::Added/RemovedComponent` 完整记账 + Propagate/Revert。  
- 范围≈新 Feature 切片；**不**作合入前默认。

**审批选项：** `[ ] A1-α` / `[ ] A1-β` / `[ ] 明确延期（接受风险，记 TECH_DEBT）`

---

### A2 — `SavePrefabAsset` 已注册路径双重写盘 + 失败语义含糊

| 项 | 内容 |
|----|------|
| **严重度** | P2（正确性边界 + 体验） |
| **现象** | 已注册 Prefab：先 `Serializer::ToFile`，再 `SaveAsset` → 再 `ToFile` |
| **证据** | `PrefabUtility::SavePrefabAsset`；`AssetManager::SaveAsset_Impl<Prefab>` |
| **触发** | Stage Save、覆盖已有 `.meprefab` |
| **影响** | 多余 I/O；若第二次失败返回 false，磁盘可能已是第一次写入 → 调用方误判「未保存」 |

#### 方案 A2-α（推荐）

- **未注册：** `ToFile` + 写 meta + `RegisterAsset(preferredGuid)`；成功后 `CacheCreatedAsset`（与 ASSET-F03 一致）。
- **已注册：** **只** `SaveAsset<Prefab>`（单一写盘路径）。
- 回归：现有 `test prefab` Guid / unpack / writeback。

#### 方案 A2-β

- 始终只 `ToFile`，已注册时只更新 meta/cache，不走 `SaveAsset`。  
- 与其它资产类型 Save 习惯可能分叉 → 不如 α。

**审批选项：** `[ ] A2-α` / `[ ] A2-β` / `[ ] 延期`

---

### A3 — 「Open Editor Scenes」只扫 `SceneManager::GetEditorScene()`

| 项 | 内容 |
|----|------|
| **严重度** | P3（当前单 EditorScene 下多为文档/命名债；ED-F11 多文档后升 P2） |
| **现象** | `FindInstanceRefsInOpenEditorScenes` / `PropagateDefaultsToOpenScenes` 名称为复数 scenes，实现只扫一个 |
| **证据** | `PrefabUtility.cpp`；`PrefabOverrideUtility::PropagateDefaultsToOpenScenes` |
| **影响** | 多开 Scene 文档时：删 Prefab 门禁漏扫、Save 后 Propagate 漏推 |

#### 方案 A3-α（最小）

- 改名 + 注释改为 `…InEditorScene` / `…ToEditorScene`，与 F26 Out（不扫磁盘）对齐。  
- **零行为变化。**

#### 方案 A3-β（行为增强）

- 遍历 Editor Document Host 已打开的 Scene 会话（依赖 ED-F11 API）。  
- 仅当 Host 能枚举多 Scene 时做；否则等同 α。

**审批选项：** `[ ] A3-α` / `[ ] A3-β` / `[ ] 延期`

---

### A4 — 已登记：Create Prefab → 全盘 `ScanAssets`（BUG-ASSET-001）

| 项 | 内容 |
|----|------|
| **严重度** | S2 / 体验（见 bug 记录） |
| **归类** | **功能缺口（Editor 写盘契约）**，非 Prefab 核心逻辑错误 |
| **方案** | 见 [BUG-ASSET-001](../../bugs/BUG-ASSET-001.md)：`SavePrefabAsset` 挂 `NoteEditorFilesystemMutation`；必要时收紧 directory 全扫阈值 |

**审批选项：** `[ ] 合入前修` / `[ ] 合入后修` / `[ ] 保持 Open`

---

### A5 — 潜在缺口（需再验后再升「确认」）

| ID | 假设 | 验证方法 | 若成立则方案 |
|----|------|----------|--------------|
| A5a | `TryRebuildEditCloneMapFromParallelTrees` 按下标对齐，在 Stage reparent 后误映射 | 破坏/清空 map 根项后 Save；或改子树顺序再走 rebuild | rebuild 改为「同名+组件类型指纹」或 **禁止 rebuild、直接失败** |
| A5b | Stage `WriteStageTree` 成功、`SavePrefabAsset` 失败 → 内存与磁盘分叉 | 只读目标路径 / 注入 Save 失败 | Save 失败时从磁盘 Reload 资产或保留 previousTemplates 直至双成功 |
| A5c | `ResolvePrefabAsset` 返回裸指针依赖 OM 强引用 | 代码已证实 OM 为 `shared_ptr`；未来弱引用会炸 | API 改为返回 `shared_ptr<Prefab>`（预防性） |

**审批选项：** `[ ] 先写复现单测再定` / `[ ] 合入后观察` / `[ ] 忽略直至复现`

---

### 明确 **不算** 本报告功能缺口（已 Out）

- Nested Prefab / Variant / Apply to Prefab / Inspector override 蓝字  
- Instantiate 世界偏移默认策略（Amendment C 后置）  
- 未打开 `.mescene` 磁盘引用扫描（F26 Out）  
- 并排 Level+Prefab 实时预览（隔离 RT）

---

## 2) 冗余 / 结构债（B）

### B1 — `WriteStageTreeToPrefab` 回滚块复制 ×3

| 项 | 内容 |
|----|------|
| **严重度** | P3（维护性；行为目前正确） |
| **位置** | `PrefabUtility.cpp` 约三处 identical restore |
| **风险** | 以后只改一处 → 半写入 Prefab |

#### 方案 B1

- 文件内抽 `RestorePreviousTemplates(prefab, previousTemplates, previousRootGuid)`（成员或同 TU 辅助）。  
- **行为不变**；与 A2 同 PR 成本最低。

**审批：** `[ ] 做` / `[ ] 不做`

---

### B2 — Guid 查找三套平行实现

| 项 | 内容 |
|----|------|
| **位置** | `FindSharedObjectInScene`（Utility 匿名）· `FindTemplateObject`（Override 匿名）· `IsTemplateObject`（Utility） |
| **问题** | 线性扫 GO+Components 拷贝三份；后续加索引易漏 |

#### 方案 B2-α（轻）

- 抽 `PrefabObjectLookup.h/.cpp`：`FindInPrefab` / `FindInSceneByGuid`；三处改调用。

#### 方案 B2-β（重 · 不做默认）

- Scene/Prefab 侧 Guid→Object 索引。收益要有规模数据才值。

**审批：** `[ ] B2-α` / `[ ] 延期`

---

### B3 — `PrefabUtility.cpp` God file（~1068 行）

| 职责簇 | 建议归属 |
|--------|----------|
| Create / Instantiate / clone | 保留 `PrefabUtility` 或 `PrefabFactory` |
| Stage writeback + EditCloneMap rebuild | `PrefabStageWriteback`（Runtime，Editor 调用） |
| Unpack / FindRefs / FormatMessage | `PrefabInstanceRegistry` 或留 Utility 尾部 |
| Save/Load 磁盘 | 更靠近 `AssetManager` / 薄 wrapper |

#### 方案 B3

- **只拆文件 + 转发**，不改对外 API 名（`PrefabUtility::*` 可 using/inline 转发）。  
- 单独 `chore(prefab): split PrefabUtility` 提交，便于回滚。

**审批：** `[ ] 合入前拆` / `[ ] 合入后拆` / `[ ] 不拆`

---

### B4 — mapping 构建双函数镜像

- `BuildInstanceRecordFromCloneContext` vs `BuildInstanceRecordAfterCreate`（仅 Template/Instance 对调）。  
- **方案：** 一个 `BuildMappings(cloneContext, EMappingDirection)`。  
- **审批：** `[ ] 随 B1/B3` / `[ ] 不做`

---

### B5 — 前向枚举未使用

- `EPrefabOverrideKind::AddedComponent` / `RemovedComponent` 已进反射/序列化，无读写。  
- **方案 α：** 保留 + 注释「F24+ reserved」（现状可接受）。  
- **方案 β：** 删枚举值（破坏已存盘 Scene 若有脏数据 — 当前应无）。  
- **审批：** `[ ] 保留+注释` / `[ ] 删除`

---

### B6 — Editor 双轨策略未完全接线

- Runtime `PrefabEditValidator` + Editor `PrefabEditConstraints` 分工合理。  
- 冗余感来自 **组件路径未接到 Validator**（归 **A1**，不单开 B 项修复）。

---

## 3) 建议执行包（供勾选）

### 包 R1 — 合入前最小修复（推荐默认）

- [ ] **A1-α** 组件 Add/Remove 门禁  
- [ ] **A2-α** Save 单写盘  
- [ ] **B1** 回滚去重（顺带）  
- 验证：`test prefab` + `test prefab-overrides`；手验 Stage Save、实例删组件应失败  

### 包 R2 — 合入前体验

- [ ] **A4** BUG-ASSET-001  
- 可与 R1 同 PR 或紧随  

### 包 R3 — 合入后 chore

- [ ] **A3-α** 命名对齐（或 β）  
- [ ] **B2-α** Lookup 合并  
- [ ] **B3** 拆文件  
- [ ] **B4** mapping 合并  
- [ ] **A5*** 按需单测  

### 包 R0 — 只合入、本期不修

- [ ] 接受 A1–A2 风险；ACTIVE_WORK / TECH_DEBT 记账  

---

## 4) 验证基线（任何批准包落地后）

| 命令 | 期望 |
|------|------|
| `minEngineTests.exe test prefab` | 全绿（含新增门禁/Save 用例） |
| `minEngineTests.exe test prefab-overrides` | 全绿 |
| 手验（若动 Editor） | Stage Save；Level 实例 Remove 组件被拒；Create Prefab（若修 A4）无全盘 Scan 风暴 |

---

## 5) 审批记录

| 日期 | 决定 | 备注 |
|------|------|------|
| 2026-09-17 | **全包**（R1+R2+R3 + A5） | 维护者：「直接完成全包的重构即可，不需要分开」 |

---

## 6) 代码体量快照（Review 时 / 落地后）

| 文件 | Review 时 | 落地后 | 角色 |
|------|-----------|--------|------|
| `PrefabUtility.cpp` | ~1068 | ~Create/Instantiate/Save | 主入口 |
| `PrefabUtility_StageWriteback.cpp` | — | WriteStage | Stage 写回 |
| `PrefabUtility_InstanceOps.cpp` | — | Unpack/Refs | 实例表 |
| `PrefabUtilityDetail.*` | — | 共享 helper | Clone/mapping/rollback |
| `PrefabObjectLookup.*` | — | Guid 查找 | 去重 |
| `PrefabOverrideUtility.cpp` | ~572 | shared_ptr Resolve | Record/Revert/Propagate |

---

## 7) 落地记录（2026-09-17）

| ID | 处置 |
|----|------|
| **A1-α** | `PrefabEditValidator` 禁 Add/RemoveComponent；`PrefabEditConstraints` + `SceneEditor` 接线 |
| **A2-α** | 已注册只 `SaveAsset`；新建 `ToFile`+meta+Register |
| **A3-α** | `FindInstanceRefsInEditorScene` / `PropagateDefaultsToEditorScene`；旧名 inline 转发 |
| **A4** | `SavePrefabAsset` 对父目录/文件/meta 调 `EditorFilesystemMutationPass`；BUG-ASSET-001 → Fixed |
| **A5a** | 去掉 BFS 下标 rebuild；map 丢失 fail-closed |
| **A5b** | Stage Save：磁盘失败则 `RestoreTemplateObjects` + 恢复 EditCloneMap |
| **A5c** | `ResolvePrefabAsset` → `shared_ptr<Prefab>` |
| **B1** | `PrefabUtilityDetail::RestorePreviousTemplates` |
| **B2-α** | `PrefabObjectLookup` |
| **B3** | Utility 拆成 3 个 `.cpp` + Detail |
| **B4** | `BuildInstanceRecordFromCloneContext` + `EMappingDirection` |
| **B5** | 枚举注释 Reserved |

**验证：** `test prefab` 16/16（195 assertions）；`test prefab-overrides` 5/5（56 assertions）；`Editor`+`minEngineTests` 构建通过。
