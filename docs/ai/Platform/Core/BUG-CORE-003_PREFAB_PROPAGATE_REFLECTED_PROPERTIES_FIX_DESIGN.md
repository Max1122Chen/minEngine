# BUG-CORE-003 + CORE-F24 收口 — Prefab Propagate / Override 编辑挂钩 — Fix Design

## Meta
- **ID:** BUG-CORE-003（Fix Design；范围扩至 F24 未按时交付项）
- **Type:** Bug fix + Feature completion design
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-16
- **Branch:** `feat/prefab`
- **Related:**
  - [Bug Record](../../bugs/BUG-CORE-003.md)
  - [CORE-F24](./CORE-F24_PREFAB_OVERRIDES_DESIGN.md) Amendment A / B
  - [CORE-F23](./CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md)（Instantiate 偏移 **Out**）
  - [ASSET-F03](../../Asset/ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)
  - [ED-F16](../../Editor/ED-F16_PREFAB_EDITOR_DESIGN.md)（Inspector 蓝字后置；本修只接记 Override）

## TL;DR

**同意的 003 方案：** Propagate 按 mapping 扫反射叶子（跳过根 Transform / override / 危险指针类）。  
**一并收口 F24 半截交付：** Editor 属性编辑 → `TryRecordPropertyOverride`；ValidateEdit 接到破坏性命令；RevertInstance 冒烟；传播与记 override 单测补齐。  
**当前：** **Done** — Propagate / TryRecord / ValidateEdit 已落地；`test prefab-overrides` 5/5。

---

## Scope

### In（本包一次做完）

| # | 项 | 对应 F24 |
|---|-----|----------|
| A | `PropagateDefaultsToScene` / `CopyNonRootPropertiesFromTemplate` → **全反射叶子** | S03 真交付 + BUG-003 |
| B | Level（及可编辑 Prefab 实例）Inspector/`AssignProperty` 成功后调 **`TryRecordPropertyOverride`** | S02 编辑挂钩 |
| C | Hierarchy 删 GO / 重挂等破坏性路径调 **`ValidateEdit`**，拒绝则不执行 | S05 规则接入 Editor |
| D | 单测：非 Transform 传播；override 跳过；根 Transform 跳过；RevertInstance smoke；可选 Scene Overrides 往返 | S03/S04/S06 补洞 |
| E | Prefab Stage Save 后 Propagate 保持现挂钩；Level Document Scene 若被改脏 → `MarkSceneDirty` | ED-F16 已有 Save 钩 |

### Out（明确不进本包）

| 项 | 归属 |
|----|------|
| Instantiate `WorldTransform` 相对模板根偏移 / Scale 相乘 | F23 Amendment C（用户接受现状） |
| AddedComponent / RemovedComponent **行为**（枚举已有） | F24 O3 / 后置 |
| Apply instance → Prefab | F24 O2 |
| 未打开 Scene 磁盘传播 | F24 O4 |
| Inspector override 蓝字 / Revert 菜单 UI | ED-F16 |
| 删 Prefab 资产断链 | CORE-F26 |
| 全盘 ScanAssets | BUG-ASSET-001 |

---

## 1) 背景：为何「Done」却不能用

对照 F24 原切片（2026-09-16 盘点）：

| 切片 | 设计 | 代码现实 |
|------|------|----------|
| S01 PropertyPath / Upsert | 要 | **有** |
| S02 编辑挂钩 TryRecord | 要 | API + **仅测试调用**；Editor **零引用** |
| S03 全属性 Propagate | 要 | **仅 Name + 非根 Transform** |
| S04 Revert | 要 | API 有；RevertInstance 薄 |
| S05 ValidateEdit | 要 | Runtime 有；**Editor 命令未系统询问** |
| S05 可选 Added/Removed | 可选 | 枚举 only → **本包仍 Out** |

手验「改 Prefab default 旧实例不动」= A；「关卡改实例不记 override」= B。两处都修才形成 F24 闭环。

---

## 2) 现状（代码要点）

### 2.1 Propagate（A）

```text
PropagateDefaultsToScene
  Guid 过滤 OK
  GO → m_Name only
  SceneComponent → m_Transform（根跳过）
  其它 ME_PROPERTY → never
```

`PropagateDefaultsToOpenScenes` → `GetEditorScene()` 通。

### 2.2 TryRecord（B）

`PrefabOverrideUtility::TryRecordPropertyOverride` 完整；`PrefabOverridesTest` 手调。  
Editor：`SceneEditorInspectorSource` → `AssignProperty` / `PostEditChangeProperty` / Undo `EditorSetObjectPropertyCommand` — **无** `TryRecord`。

### 2.3 ValidateEdit（C）

`PrefabEditValidator`：禁删根、禁实例内重挂、禁第二顶层等。  
Hierarchy 删/重挂路径需核对是否调用；本包要求：**凡改 Prefab 实例结构的 Editor 命令在 Execute 前 ValidateEdit**。

---

## 3) 方案

### 3.1 A — 全属性 Propagate（已拍板，用户同意）

```text
For each mapping (templateGuid → instanceGuid):
  resolve templateObj / instanceObj
  For each reflected leaf on that object:
    skip Instanced 容器（子对象走自己的 mapping）
    skip IsRootTransformPath
    skip HasOverride(record, templateGuid, localPath)
    skip Delegate / 非资产 ObjectPtr（O2）
    SerializePathToPayload(template) → ApplyPayloadToPath(instance)
Mark Scene dirty if any write
```

`CopyNonRootPropertiesFromTemplate`（RevertInstance）与 Propagate **共用同一套遍历**，避免两套白名单再漂移。

### 3.2 B — Editor 记 Override

**挂钩点（默认）：** 属性写入成功且 Scene 为 Editor、对象属于某 `PrefabInstanceRecord` 之后：

1. `SceneEditorInspectorSource` 在 `AssignProperty` 成功路径（含 Undo apply 的 after 值）  
2. 若 Console/`set` 走同一 `ApplySetObjectProperty`，一并覆盖  

```text
AssignProperty 成功
  → PrefabOverrideUtility::TryRecordPropertyOverride(scene, *owner, capturePropertyPath)
  → 失败仅 Warn（不回滚已写入值；避免 Inspector 卡死）
  → MarkSceneDirty（已有则复用）
```

**路径：** 使用 Inspector 已有 `capturePropertyPath`（对象本地名，如 `m_bCastShadows`）。与 TryRecord / HasOverride 键一致。  
**Prefab Stage：** Stage 编辑的是资产模板克隆（`bRegisterPrefabInstance=false`），**不**记 Level override；Stage 改动靠 Save→WriteStageTree→Propagate。若误调 TryRecord 找不到 record → 现 API 对 plain GO 返回 true，可保持。

**根 Transform：** 仍可记 override（F24 O1：场景摆放）；Propagate 跳过根 Transform 即可。

### 3.3 C — ValidateEdit 接入

| Editor 操作 | 校验 |
|-------------|------|
| 删除映射内 GO | `DeleteGameObject`；根拒绝；子节点 MVP 拒绝（与现 Validator） |
| 重挂 | `Reparent`；根挂到外部允许；实例内/挂出拒绝按现规则 |
| Add/Remove Component | MVP：ValidateEdit 已对 Remove Root 等有规则；**不实现** Added/Removed override 表 |

未通过：打 Error log + UI 不执行（与现 PrefabEditConstraints 风格一致）。

### 3.4 D — 测试

| Case | 断言 |
|------|------|
| propagate non-transform | 改模板 `CastShadows`（或等价）→ Propagate → 实例变；根 Position 不变 |
| propagate respects override | 实例已 override Name → 模板改 Name → 实例名不变 |
| editor record（可测 Runtime 模拟） | TryRecord 后 HasOverride；值改回 default → Remove |
| RevertInstance | 清 Overrides + 非根字段回模板；根 Transform 保留 |
| （可选）Scene JSON | Overrides 数组往返 |

### 3.5 切片建议（实现时）

| Slice | 内容 | 验证 |
|-------|------|------|
| **S01** | A：反射 Propagate + 共用 Copy 路径 | `test prefab-overrides` |
| **S02** | B：Inspector/ApplySet 挂钩 TryRecord | 手验 + 可选单测 |
| **S03** | C：删/重挂 ValidateEdit | 手验 + 既有 validate 测 |
| **S04** | D：RevertInstance + DoD 文档 | 单测；Bug → Fixed；F24 验收条改真勾 |

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| 只修 Propagate，Editor 挂钩另开 bug | **拒绝**；用户要求本包补 F24 未交付 |
| Added/Removed 一并做 | **拒绝**；原 S05 可选，组合爆炸大 |
| 传播整对象二进制覆盖 | **拒绝** |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 误拷指针/Delegate | O2 跳过类别；单测资产引用 |
| Undo 后 Override 表与值不一致 | after **与** before 应用后都 TryRecord（或 Revert 路径显式同步） |
| ValidateEdit 过严挡合法摆放 | 仅结构命令；属性编辑不走 C |
| 性能 | 开 Scene × 实例 × 属性；MVP OK |

---

## 6) 验收标准

- [x] Prefab Stage 改非 Transform default → Save → 开着的 Level 旧实例更新
- [x] Level Inspector 改实例属性 → `Overrides` 出现；改回 default → 条目消失；存盘再开保留
- [x] 有 override 的字段不被 Propagate 覆盖；根 Transform 不传播
- [x] 删 Prefab 实例根被拒；合法操作仍可用
- [x] `test prefab` / `test prefab-overrides` 绿（含新 case）
- [x] BUG-CORE-003 → Fixed；F24 §6 传播/挂钩验收改为真实完成（Added/Removed 仍标可选 Out）

---

## 7) 开放点（本包默认）

| # | 问题 | 默认 |
|---|------|------|
| O1 | Instanced 子对象只走 mapping | **是** |
| O2 | 跳过 Delegate / 非资产 ObjectPtr | **是**；资产 Guid 引用可拷 |
| O3 | 传播后 MarkDocumentSceneDirty | **是** |
| O4 | Undo 后是否再 TryRecord | **是**（before/after 各一次或等价） |
| O5 | Console `set` 是否记 Override | **是**（与 Inspector 同 Apply 路径则自动覆盖） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-16 | Review：仅全属性 Propagate |
| 2026-09-16 | **扩范围 Review：** 用户同意 003；并入 F24 S02 挂钩 + S05 Editor Validate + 测试收口；Added/Removed 仍 Out |
| 2026-09-16 | **Done：** 实现；prefab-overrides 5/5、prefab 14/14 |
