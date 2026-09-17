# CORE-F24 — Prefab Overrides + Default Propagation — Design Spec

## Meta
- **ID:** `CORE-F24`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-16
- **Branch:** `feat/prefab`
- **Related:**
  - [CORE-F23 Prefab Asset + Instantiate](./CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md)（**硬依赖**）
  - [ED-F16 Prefab Editor](../../Editor/ED-F16_PREFAB_EDITOR_DESIGN.md)（消费 override 可视化；本 Feature 可无完整 UI）
  - [ENGINE_DESIGN_PHILOSOPHY](../../ENGINE_DESIGN_PHILOSOPHY.md)
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - [BUG-CORE-003](../../bugs/BUG-CORE-003.md)（传播表面积）
- **Depends on:** CORE-F23 **Done**（类名 `Prefab`、模板 Guid、`PrefabInstanceRecord`、Instantiate 映射）
- **Blocks:** 有意义的 Prefab 工作流（改资产 → 实例跟随）；ED-F16 Inspector 区分 default/override

## TL;DR

在 F23（**Done**）的空 `PrefabInstance` 之上，支持 **受限的编辑期实例 Override**，以及 **Prefab 模板 default 变更向未覆盖字段传播**。

> **2026-09-16：** Override / Revert / 根 Transform 排除成立。**Propagate 全属性 + Editor TryRecord + Level ValidateEdit** 已由 [BUG-CORE-003](../../bugs/BUG-CORE-003.md) **Fixed** 收口。Added/RemovedComponent、Apply→Prefab、磁盘传播仍 Out。

- 运行时 / PIE 仍烘焙为普通 GO（应用 override 后无 Prefab 身份）。
- **不是**完整 Unity：禁止部分层级编辑；允许 property override、有限的 add/remove component。
- Override 键使用 **`(PrefabAssetGuid, TemplateObjectGuid, PropertyPath)`**，不用世界 Guid。

## Scope

### In（CORE-F24）

- `PrefabPropertyOverride` 完整载荷与比较 / 应用 / 回写
- 实例上编辑属性 → 记录 override（相对当前模板 default）
- 修改 Prefab 资产 default → 传播到各 Scene 中未 override 的对应字段
- 受限规则表：哪些层级操作禁止、哪些 component 操作允许
- Revert override（单属性 / 整实例）API
- 单测：`test prefab-overrides`（或扩展 `test prefab`）
- 可选：极薄 Editor 指示（蓝字 / 星号）可延后到 ED-F16；**本 Feature 以 Runtime/Editor 共享机制为主**

### Out

| 项 | 归属 |
|----|------|
| Prefab Mode / Stage Scene | **ED-F16** |
| Nested Prefab、Prefab Variant | 后置 |
| 任意重排整棵实例树 / 换根 | **禁止（本 Feature 明确拒绝）** |
| 运行时动态 override API（Gameplay 改 override 表） | Out；Gameplay 直接改 GO |
| 完整 Unity Applied / Unapplied 全矩阵 UI | ED-F16 子集 |

## Reader quick start

1. §3.1 模型 · §3.2 数据结构 · §3.3 规则表 · §3.4 传播算法 · §3.5 API  
2. §5 风险 · §6 验收 · §7 切片  

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 依赖 | F23 **必须 Done**；反射 AssignProperty / 序列化 **sound** |
| 债风险 | **High if rushed** — 差分格式与规则表一次定错会长期痛；故 F23 之后单独 Feature |
| WIP | 勿与 F23 同切片开工 |
| 哲学 | 机制（差分 + 传播）在 Framework；策略（允许哪些编辑）用显式规则表，避免隐式「全能」 |
| 建议 | **Go after F23**；本 Draft 先锁模型，实现前再 Review 规则表 |

---

## 1) 背景与目标

### 1.1 Pain

- F23 只能「改资产文件 → 手动再 Instantiate」；已有实例不更新。
- 关卡需要「同一 Prefab，个别灯强度不同」——无 override 只能拆开复制，失去统一改模板的能力。

### 1.2 Goals

1. 实例相对模板的 **属性级** 差分可存盘、可应用、可还原。
2. 改 Prefab default 后，**未 override** 的实例字段自动跟随；已 override 的保留。
3. 用规则表限制危险结构变更，降低与层级系统的组合爆炸。
4. 烘焙进 PIE/Runtime 后仍是普通 GO。

### 1.3 Success

- 单测覆盖：写 override → 存盘 → 加载 → 值保留；改 default → 未覆盖字段变、覆盖字段不变；Revert 后跟随新 default。
- 违规层级操作被拒绝并说明原因。

---

## 2) 现状（F23 已 Done）

- `Prefab` 类 + `.meprefab` 管线；`PrefabUtility` Create/Instantiate
- `PrefabInstanceRecord` 含空 `Overrides`；字段 `PrefabAssetGuid`（资产 Guid，非类名）
- `ObjectMappings`：TemplateGuid ↔ InstanceGuid
- 尚无差分比较器、无传播管线（本 Feature 范围）

---

## 3) 方案

### 3.1 概念

```text
Prefab (defaults / template)
        │
        │ Instantiate / Open Scene
        ▼
Scene PrefabInstanceRecord
  mappings + overrides[]
        │
        │ ApplyOverrides (editor load / after propagate)
        ▼
Instance GameObjects (concrete values)
        │
        │ PIE bake
        ▼
Plain GameObjects (no record)
```

**Default：** 模板对象上该属性的当前值。  
**Override：** 实例记录中「此属性不以 default 为准」的条目。  
**Propagation：** 模板属性变化时，对所有引用该资产的实例：若无该属性 override，则写入新 default。

### 3.2 数据结构

在 F23 骨架上补全：

```cpp
enum class EPrefabOverrideKind : uint8_t
{
    PropertyValue = 0,
    AddedComponent,      // optional phase
    RemovedComponent,    // optional phase
};

ME_STRUCT()
struct PrefabPropertyOverride
{
    ME_PROPERTY()
    EPrefabOverrideKind Kind{ EPrefabOverrideKind::PropertyValue };

    /** Template-side object this override targets. */
    ME_PROPERTY()
    GUID TemplateObjectGuid;

    /**
     * Reflection path from that object.
     * Examples:
     *   "m_Name"
     *   "m_Components/<TemplateComponentGuid>/m_Intensity"
     * Prefer Guid-stable addressing for components over list index.
     */
    ME_PROPERTY()
    std::string PropertyPath;

    /**
     * Serialized value payload (JSON fragment or engine archive blob).
     * Must round-trip through same type as the property.
     */
    ME_PROPERTY()
    std::string ValueJson;

    /** For AddedComponent: class name / type id. */
    ME_PROPERTY()
    std::string TypeName;

    /** For AddedComponent: new instance Guid (stable in this Scene record). */
    ME_PROPERTY()
    GUID AddedInstanceGuid;
};
```

**PropertyPath 稳定策略（默认拍板）：**

- 标量 / 结构字段：反射名路径（与 Serializer path 风格一致）。
- Component 上字段：`m_Components/{componentTemplateGuid}/m_Field`，**禁止**只用 `m_Components[2]`（模板增删 component 会错位）。
- GO 引用型字段：存 **模板 Guid** 或实例 Guid？→ 存盘 override 值里写 **实例 Guid**（应用时已在实例世界）；比较 default 时把模板侧引用 remap 到实例再比。

### 3.3 允许 / 禁止规则表（MVP）

#### 3.3.1 允许

| 操作 | 说明 |
|------|------|
| 改实例上已有属性（反射可写、非 Transient） | 记 PropertyValue override |
| 改根 Transform（世界/局部按编辑器惯例） | override；传播时注意「根位置常被场景摆放」——见 O1 |
| AddComponent（非第二 Root SceneComponent） | `AddedComponent`；仅挂在已有实例 GO 上 |
| RemoveComponent（非 Root SceneComponent、非强制组件） | `RemovedComponent` |

#### 3.3.2 禁止（MVP）

| 操作 | 原因 |
|------|------|
| 删除 / 替换 Prefab 根 GO | 断开实例身份 |
| 把实例根挂到另一 Prefab 实例内部形成嵌套 | Nested 后置 |
| 改 `m_Parent` 使节点移出实例子树或移入外部 | 结构漂移 |
| 在实例内引入「第二个顶层根」 | 破坏单根 |
| 删掉映射表中的模板节点对应 GO | 结构 override 过强；MVP 不做 |

实现：`PrefabEditValidator::CanApply(Scene&, PrefabInstanceRecord&, EditOp) → Error`。  
Editor 命令在提交前询问；脚本 API 同样走校验。

#### 3.3.3 根 Transform 策略（开放点 O1 默认）

**默认：根 Transform 始终视为场景摆放 override**（Instantiate 时写入；Create 原地转实例时保留当时世界变换）。  
改 Prefab 模板根 Transform **不** 传播到已有实例根（避免刷关卡位置被拽回原点）。  
子节点 local Transform 的 default **要**传播。

### 3.4 算法

#### 3.4.1 记录 Override（编辑属性后）

```text
OnPropertyEdited(instanceObject, propertyPath, newValue):
  record = FindInstanceRecord(scene, instanceObject.Guid)
  if !record: treat as plain GO; return
  templateObj = MapToTemplate(record, instanceObject)
  defaultValue = ReadProperty(templateObj, propertyPath)  // load asset if needed
  if ValuesEqual(newValue, defaultValue):
       RemoveOverride(record, templateObj.Guid, propertyPath)
  else:
       UpsertOverride(record, { templateObj.Guid, propertyPath, Serialize(newValue) })
  MarkSceneDirty
```

#### 3.4.2 应用 Override（加载 Scene / 刷新）

```text
For each PrefabInstanceRecord:
  Ensure instance tree matches mappings (F23)
  // optional: reset instance properties from template (expensive) OR assume disk already concrete
  For each override in order:
       Apply to instance object via AssignProperty / component add-remove
```

**存盘策略（默认拍板）：** Scene JSON **存具体属性值 + Overrides 表**（冗余但加载快、手改友好）。  
加载时：以磁盘具体值为准；Overrides 用于传播 / Revert / UI。  
传播时：只写「无 override」的字段。

备选：Scene 只存 override、加载时从 Prefab 重建 — 更纯，但依赖资产始终可得。**MVP 选冗余具体值 + Overrides 表。**

#### 3.4.3 Default 传播

```text
PropagatePrefabDefaults(Prefab& asset):
  For each open Scene (and optionally dirty assets on disk — MVP: open scenes only):
    For each PrefabInstanceRecord where PrefabAssetGuid == asset.Guid:
      templateSnapshot = asset
      For each mapping (templateGuid → instanceGuid):
        For each reflected property on template object:
          if IsRootTransformExcluded(...): continue
          if HasOverride(record, templateGuid, path): continue
          if !ValuesEqual(instance.prop, template.prop):
             Assign(instance.prop, template.prop)
             Mark dirty
```

磁盘上未打开的 Scene：**F24 MVP 不扫全项目**（避免无静默改盘）；可提供显式命令 `PropagateToAssetOnDisk(path)` 后置。

**实现缺口（2026-09-16，≠ Guid）：** `PropagateDefaultsToScene` **没有**「For each reflected property」。当前只拷 GameObject `m_Name` 与 SceneComponent `m_Transform`（根再跳过）。因此 Instantiate（整树克隆）能带上新 default，已有实例在 Prefab Save 后不更新。挂钩 `PropagateDefaultsToOpenScenes` → `GetEditorScene()` 是通的。修复见 [BUG-CORE-003 Fix Design](./BUG-CORE-003_PREFAB_PROPAGATE_REFLECTED_PROPERTIES_FIX_DESIGN.md)。

#### 3.4.4 Revert

- `RevertProperty(record, templateGuid, path)`：删 override，从模板写回实例。  
- `RevertInstance(record)`：清空 Overrides，整树从模板刷新（保留根 Transform 策略）。  
- `ApplyAllToPrefab`（把实例改动写回模板）：**F24 可选后置**；与 ED-F16 Apply 强相关。默认 **先不做 Apply to Prefab**，只做 Revert + 传播。

### 3.5 API

```cpp
class PrefabOverrideUtility
{
public:
    static bool TryRecordPropertyOverride(
        Scene& scene,
        MEObject& instanceObject,
        std::string_view propertyPath,
        std::string* outError = nullptr);

    static bool RevertProperty(
        Scene& scene,
        MEObject& instanceObject,
        std::string_view propertyPath,
        std::string* outError = nullptr);

    static bool RevertInstance(
        Scene& scene,
        GUID rootInstanceGuid,
        std::string* outError = nullptr);

    /** After Prefab saved; updates open editor scenes. */
    static void PropagateDefaultsToOpenScenes(const Prefab& asset);

    static bool HasOverride(
        const PrefabInstanceRecord& record,
        const GUID& templateObjectGuid,
        std::string_view propertyPath);

    static PrefabEditValidationResult ValidateEdit(
        Scene& scene,
        const PrefabInstanceRecord& record,
        const PrefabEditOp& op);
};
```

挂钩点：

- Editor `AssignProperty` / `PostEditChangeProperty` → `TryRecordPropertyOverride`（仅当对象属于某 PrefabInstance）
- Prefab Save（ED-F16 或 Content Browser）→ `PropagateDefaultsToOpenScenes`

### 3.6 数据流（编辑期）

```text
User edits instance light intensity
  → AssignProperty
  → PrefabOverrideUtility::TryRecordPropertyOverride
  → Overrides upsert / remove if equal default

User edits Prefab asset default (hand-edit or future Prefab Mode)
  → Save Prefab
  → PropagateDefaultsToOpenScenes
  → instances without override update

Play
  → PIE duplicate
  → clear PrefabInstances
  → GO values already concrete (includes applied overrides)
```

### 3.7 文件布局

```text
Runtime/Function/Framework/Prefab/
  PrefabTypes.h              // extend override structs
  PrefabOverrideUtility.*
  PrefabEditValidator.*
  PrefabPropertyPath.*       // path parse / component-guid segment
  PrefabPropagation.*
```

### 3.8 与 ED-F16 的分工

| 机制（F24） | 呈现（F16） |
|-------------|-------------|
| Overrides 表、传播、Revert API | Inspector 标记 override、右键 Revert |
| ValidateEdit | Prefab Mode / Scene 模式禁用菜单项 |
| Propagate on Save | Prefab 文档 Save 成功回调 |

F24 **可以**在无 F16 时用单测 + 手写 JSON 验收。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| Scene 只存 override | 无冗余 | 丢资产即坏 Scene | MVP **不用** |
| Scene 存具体值 + Overrides | 稳、可手改 | 可能不一致 | **选用**；传播/Revert 维护一致 |
| 运行时保留 PrefabInstance | 像 Unity | 违背「Runtime 普通 GO」 | **拒绝** |
| 索引路径 `m_Components[i]` | 简单 | 增删即碎 | **拒绝**；用 component 模板 Guid |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| PropertyPath 不稳定 | 静默错绑 | Guid 段 + 单测；禁止纯下标 |
| 传播扫全项目 | 慢、危险 | MVP 仅 open Scene |
| 根 Transform 误传播 | 关卡物体归零 | O1 默认排除根 Transform |
| AddedComponent 与模板新组件冲突 | Guid/顺序乱 | 规则表 + 后期再做 Nested |
| 具体值与 Overrides 不一致 | 难查 | Revert/Propagate 后断言；加载可选校验模式 |

---

## 6) 验收标准

- [x] 改实例属性 → Overrides 出现；Revert 回 default → 条目移除（`test prefab-overrides` record/revert）
- [ ] Scene 存盘重开：覆盖值保留；Override 表完整（API/结构已序列化；**未做专用 round-trip 单测**，可后置）
- [x] 改 Prefab default → 无 override 字段更新；有 override 不变；根 Transform 不传播  
  （BUG-CORE-003：**全反射叶子** + Name/子 Transform + CastShadows 单测）
- [x] RevertProperty 正确；RevertInstance smoke（`test prefab-overrides`）
- [x] 禁止删除实例根返回明确错误（ValidateEdit；Editor Hierarchy 已接）
- [x] PIE 无 PrefabInstance 依赖：沿用 F23（PIE 清表）；本 Feature 不新增运行时身份
- [x] `test prefab-overrides` 5/5；`test prefab` 14/14 全绿

---

## 7) 建议实现切片

| Slice | 内容 | 验证 |
|-------|------|------|
| **S01** | PropertyPath + ValuesEqual + Upsert/Remove override | 单测 |
| **S02** | 编辑挂钩 TryRecord；Scene 往返 | 单测 + 手工 |
| **S03** | PropagateDefaultsToOpenScenes + 根 Transform 排除 | 单测 |
| **S04** | Revert APIs | 单测 |
| **S05** | ValidateEdit 层级禁止；可选 Added/RemovedComponent | 单测 |
| **S06** | DoD / 文档 | — |

---

## 8) 开放点（设计默认已拍板；实现前可再 Review 规则表细节）

| # | 问题 | 默认 |
|---|------|------|
| O1 | 根 Transform 是否随 default 传播 | **否**（始终场景摆放） |
| O2 | Apply changes to Prefab | **F24 Out**；ED-F16 再议 |
| O3 | Added/Removed component | **S05 可选**；可先只做 PropertyValue |
| O4 | 未打开 Scene 的磁盘传播 | **Out** |
| O5 | Override 值存 JSON 字符串 vs Binary blob | **落地：`ValueJson` = `"bin:"` + hex**（属性二进制缓冲）；字段名保留兼容 |
| O6 | 传播是否扫全部反射叶子 | **是（BUG-CORE-003 Done）** |

---

## 9) Amendment A — 传播表面积（2026-09-16）

曾用白名单代替全属性遍历。**已由 BUG-CORE-003 Fixed 纠正。**

---

## 10) Amendment B — F24 收口包（与 BUG-CORE-003）

| 交付 | 状态 |
|------|------|
| S03 真传播 | **Done** |
| S02 Editor TryRecord | **Done**（`ApplySetObjectProperty`） |
| S05 Editor ValidateEdit | **Done**（Level 删/重挂） |
| 测试收口 | **Done**（`prefab-overrides` 5/5） |

**仍 Out：** Added/RemovedComponent 行为、Apply→Prefab、未打开 Scene 磁盘传播、Instantiate 偏移、Inspector 蓝字 UI。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | Draft：受限 override、传播、规则表、与 F23/F16 边界 |
| 2026-09-15 | 对齐 F23 类名 `Prefab`（非 PrefabAsset）；依赖改为 F23 Done；UTF-8 重写修复编码损坏 |
| 2026-09-15 | **Done：** PropertyValue override、Propagate（开 Scene）、RevertProperty、ValidateEdit；`test prefab-overrides`；payload=`bin:`+hex |
| 2026-09-15 | Note：Propagate 按 `PrefabAssetGuid == prefab.GetGuid()` 过滤；Create→首次 Register 若 Guid 失配会导致「改 Prefab default 源树不动」——根因与修复见 **CORE-F23 Amendment B**（非 Propagate 算法本身） |
| 2026-09-16 | **Amendment A：** 实现仅 Name+非根 Transform；全属性传播 → BUG-CORE-003 Review |
| 2026-09-16 | **Amendment B：** 003 + F24 S02/S05 收口同包 Review |
| 2026-09-16 | **BUG-CORE-003 Fixed：** 全属性 Propagate + TryRecord + ValidateEdit；Amendment A/B 收口完成 |
