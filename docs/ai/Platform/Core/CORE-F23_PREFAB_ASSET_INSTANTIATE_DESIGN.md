# CORE-F23 — Prefab Asset + Instantiate — Design Spec

## Meta
- **ID:** `CORE-F23`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/prefab`
- **Related:**
  - [CORE-F24 Prefab Overrides](./CORE-F24_PREFAB_OVERRIDES_DESIGN.md)（本期 **不做**；预留空 `PrefabInstance`）
  - [ED-F16 Prefab Editor](../../Editor/ED-F16_PREFAB_EDITOR_DESIGN.md)（本期 **不做**）
  - [ENGINE_0_1_0_ROADMAP](../../ENGINE_0_1_0_ROADMAP.md) — Prefab A / D3
  - [ENGINE_DESIGN_PHILOSOPHY](../../ENGINE_DESIGN_PHILOSOPHY.md) — Mechanism over Policy；Runtime≡Editor API
  - [CORE-F18 Schema / EngineVersion](../Serialization/CORE-F18_SCHEMA_ENGINE_VERSION_DESIGN.md)
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** Reflection Instanced 树序列化；`ObjectCloneContext` / Serializer 克隆路径；`AssetTypeRegistry`；CORE-F18 磁盘 schema
- **Blocks:** CORE-F24（override / 传播）；ED-F16（Prefab Mode）；0.1.0 Demo 刷 Prefab 实例（D3）

## TL;DR

在 `Runtime/Function/Framework/Prefab/` 落地 **Prefab 资产（以单个根 GameObject 为根的树）** + **Create from Scene** + **Instantiate into Scene**（**Status: Done**）。

- Prefab **不是** Scene；磁盘 `.meprefab`（JSON）+ `.meta`。
- Runtime / PIE 中实例是 **普通 GO**（无 Prefab 身份字段）。
- Editor Scene 存盘带 **空 override 的 `PrefabInstance` 记录**（为 F24 留格式，本 Feature 不做差分 / 传播）。
- 无 Prefab 编辑器：可用手写 / 外部工具改模板资产。

## Scope

### In（CORE-F23）

- `Prefab`（`Asset` 子类，**类名即 `Prefab`，非 PrefabAsset**）：持有根 GO 子树（Instanced）
- 资产类型注册：`AssetTypeId = "Prefab"`，扩展名 `.meprefab`
- `PrefabUtility`（或同级服务类）：Create / Instantiate / 引用切断
- 推广子树克隆：复用 / 泛化 `SceneCloneContext` 风格的 Guid remap（不必绑死 PIE）
- Scene 侧：`m_PrefabInstances`（或等价容器）— **链接 + 映射，override 列表恒空**
- Load/Save Scene 时保留实例链接；进入 PIE / cooked Runtime 时 **烘焙**：应用（空）override 后丢弃链接语义 → 普通 GO
- 单测：`test prefab`（Create / Instantiate / 树内引用 / 资产引用 / 外部引用切断）

### Out / 另轨

| 项 | 归属 |
|----|------|
| Property override、add/remove component、default 传播 | **CORE-F24** |
| Prefab Mode / Stage Scene / 复用 SceneEditor | **ED-F16** |
| Nested Prefab、Variant、Unpack 全部语义 | 后置 |
| 并排预览 / 隔离 RT | `RND-F17` 类（不挡本 Feature） |
| 改 Prefab 资产后自动推送到已有实例 | **F24** |
| Prefab 专属 Editor UI / 右键菜单（除最小 Create/Instantiate 命令若 Demo 需要） | ED-F16 或本 Feature 极薄 CLI/测试入口即可 |

## Reader quick start

1. §3.1 概念 · §3.2 数据结构 · §3.3 引用切断 · §3.4 API · §3.5 数据流  
2. §3.6 文件布局 · §3.10 不变量 · §6 验收 · §7 切片  
3. 代码落点（实现后）：`Runtime/Function/Framework/Prefab/` · `Scene` · Asset 注册

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 依赖 | Scene JSON + Instanced GO/Component **sound**；全 Scene PIE 克隆 **partial**（需子树路径）；Asset 注册 **sound** |
| 债风险 | **Medium** — 若 Prefab 做成迷你 Scene，或把链接写进 `GameObject` Runtime 字段，边界会糊 |
| WIP | master 已合 F20/F22 + editor；本轨独立 `feat/prefab` |
| 哲学 | Prefab = 可复用世界碎片机制；非 Gameplay Framework — **合** |
| 建议 | **Go with scope cut** — F23 = 资产 + Instantiate + 空链接表 |

---

## 1) 背景与目标

### 1.1 Pain

- Demo / 关卡需要「刷」同类物体（目标、弹匣等），今天只能手摆或整 Scene 复制。
- Roadmap Prefab A 要求资产 + Instantiate，且 **不**被 Prefab 子编辑器 / 隔离 RT 挡住。
- 若无稳定模板 Guid + Scene 侧实例链接，后续 Override（F24）会被迫改 Scene 存盘格式。

### 1.2 Goals

1. Prefab 资产表示一棵 **单根 GO 树**；与 Scene 类型解耦。
2. 从 Scene 选中根 GO **Create** Prefab；从 Prefab **Instantiate** 进任意 Editor Scene。
3. 模板内对象使用 **稳定 Guid**（资产内本地身份）；实例化时 remap 为世界 Guid。
4. 引用策略：保留树内互引 + 资产引用；**切断**树外非资产引用。
5. Runtime 实例与普通 GO 无区别；编辑期链接不污染 `GameObject` 公共 Runtime API。

### 1.3 Success

- `test prefab` 绿：Create → 磁盘 → Load → Instantiate；树内引用正确；外部引用为 null；资产 Guid 引用仍指向同一 Mesh/Material。
- Scene 存盘含 `PrefabInstance`（override 空）；PIE 后实例为普通 GO。
- Core Prefab 代码无 ImGui / Maximum 依赖。

---

## 2) 现状

| 能力 | 状态 | 备注 |
|------|------|------|
| Scene / GO / Component JSON Instanced | Sound | `.mescene` |
| PIE 全 Scene Binary 克隆 + Guid remap | Partial | `SceneDuplicator` + `SceneCloneContext`；无子树 API |
| AssetTypeRegistry | Sound | Scene / Material 可作插件样板 |
| Prefab 类型 / Instantiate | **Done** | `Framework/Prefab/` + `test prefab` |
| Override / Prefab Mode | Missing | F24 / F16 |

关键复用：

- `Serializer` + `GetActiveCloneContext()`：反序列化 Instanced 对象时换 Guid 并 `RecordClone`
- `Scene::InsertRestoredGameObject`：赋 Scene-local `m_ID`
- `Scene::ResolveGameObjectHierarchy` / SC attach rebuild

---

## 3) 方案

### 3.1 概念模型

```text
Prefab (disk .meprefab)
  └─ Root GameObject (template)
        ├─ Components (Instanced)
        └─ Child GameObjects… (via m_Parent / hierarchy)

Scene (editor document)
  ├─ GameObjects… (instances look like normal GOs)
  └─ PrefabInstances[]          // editor/serialization side-table
        ├─ PrefabAssetGuid
        ├─ RootInstanceGuid
        ├─ TemplateGuid → InstanceGuid map
        └─ Overrides[]          // F23: always empty

Runtime / PIE load path
  → Instantiate semantics baked → PrefabInstances discarded / ignored
  → GameObjects only
```

**术语**

| 名 | 含义 |
|----|------|
| **Template object** | Prefab 资产内的 GO/Component；其 `m_Guid` 在资产生命周期内稳定 |
| **Instance object** | Scene 中的克隆；世界 Guid 唯一 |
| **Prefab 资产 Guid** | `.meta` / 资产身份（与 Material 相同） |
| **PrefabInstance** | Scene 侧一条「此根 GO 来自某 Prefab」记录；**不是** `GameObject` 子类 |

**刻意不做：** `GameObject::m_PrefabAsset` / `IsPrefabInstance()` 等 Runtime API。编辑器通过 Scene 表 + Guid 查询。

### 3.2 数据结构

#### 3.2.1 `Prefab`

```cpp
// Runtime/Function/Framework/Prefab/Prefab.h
ME_CLASS()
class Prefab : public Asset
{
    ME_GENERATED_BODY()
public:
    GameObject* GetRootGameObject() const;
    // ...

private:
    /** Single root of the template tree. Children via GameObject hierarchy. */
    ME_PROPERTY(Instanced)
    std::shared_ptr<GameObject> m_RootGameObject;
};
```

不变量：

- `m_RootGameObject != nullptr`（空 Prefab 不允许存盘，或仅允许工具创建时短暂为空）
- `m_RootGameObject->GetParent() == nullptr`
- 树内所有 GO 的 parent 链最终指向该根；无第二顶层 GO
- Prefab 资产 **不** 嵌入 `Scene`，**不** 持有 `RenderScene` / PIE 字段

#### 3.2.2 `PrefabInstanceRecord`（Scene 侧）

```cpp
ME_STRUCT()
struct PrefabObjectMapping
{
    ME_PROPERTY()
    GUID TemplateGuid;   // object Guid inside Prefab

    ME_PROPERTY()
    GUID InstanceGuid;   // object Guid inside this Scene
};

ME_STRUCT()
struct PrefabPropertyOverride
{
    // F23: type exists for schema stability OR deferred entirely to F24.
    // Recommendation: declare empty vector type alias in F23; full fields in F24.
    ME_PROPERTY()
    GUID TemplateObjectGuid;

    ME_PROPERTY()
    std::string PropertyPath;   // e.g. "m_Components[0].m_Intensity"

    // Value payload: F24 decides (JSON fragment / typed variant). F23 unused.
};

ME_STRUCT()
struct PrefabInstanceRecord
{
    ME_PROPERTY()
    GUID PrefabAssetGuid;

    ME_PROPERTY()
    GUID RootInstanceGuid;      // instance root GO Guid in Scene

    ME_PROPERTY()
    std::vector<PrefabObjectMapping> ObjectMappings;

    ME_PROPERTY()
    std::vector<PrefabPropertyOverride> Overrides; // F23: always empty
};
```

挂在 `Scene`：

```cpp
// Scene.h — additive
ME_PROPERTY()
std::vector<PrefabInstanceRecord> m_PrefabInstances;
```

查找：`FindPrefabInstanceByRootGuid(GUID)` / `FindPrefabInstanceByInstanceObjectGuid(GUID)`（扫描 mappings）。

#### 3.2.3 克隆上下文泛化

现有 `SceneCloneContext` 面向 PIE。F23 引入更中性的命名（二选一，实现时定）：

| 选项 | 说明 |
|------|------|
| **A（推荐）** | 新增 `ObjectCloneContext`（Guid map + RecordClone + Resolve）；`SceneCloneContext` 持有 /  typedef 扩展 PIE 字段 |
| **B** | 继续用 `SceneCloneContext`，Prefab 路径不填 `PIEInstanceId` |

推荐 **A**：避免 Prefab 依赖「Scene/PIE」语义。

```cpp
struct ObjectCloneContext
{
    std::unordered_map<GUID, GUID, GUID::Hash> SourceToClonedGuid;
    std::unordered_map<GUID, std::shared_ptr<MEObject>, GUID::Hash> ClonedBySourceGuid;

    void RecordClone(const GUID& sourceGuid,
                     const std::shared_ptr<MEObject>& clonedObject,
                     const GUID& clonedGuid);
    MEObject* ResolveRef(const GUID& sourceGuid) const;
};
```

Serializer 热路径：`GetActiveCloneContext()` 返回可写入的 map（PIE 与 Prefab 共用）。

#### 3.2.4 Guid 身份契约（非第二种 Guid 类型）

| Guid | 稳定范围 | 用途 |
|------|----------|------|
| Prefab **asset** Guid | 项目资产 | `LoadAssetByGUID`、Scene `PrefabAssetGuid` |
| Template object Guid | **该 Prefab 文件内** | 树内互引；F24 override 键；映射表左侧 |
| Instance object Guid | **该 Scene / 世界** | ObjectManager、Scene 引用 |
| 共享资产 Guid（Mesh 等） | 项目资产 | **Instantiate 不 remap** |

规则：

1. Create Prefab：模板对象 Guid **可**沿用源 Scene 对象当时的 Guid（拷贝进资产后成为模板 id），或全部重新生成一轮稳定 id — **推荐重新生成模板 Guid**，避免与源 Scene 对象 Guid 碰撞（同一 ObjectManager 会话内）。源 Scene 实例随后换成新世界 Guid 并写入 mapping（若 Create 后原地变成实例）。
2. 打开 / 保存 Prefab 资产：**不得**无故更换模板 Guid（否则 F24 键失效）。
3. Instantiate：每个模板 MEObject → `GenerateGUID()` 新实例 Guid + `RecordClone`。

#### 3.2.5 磁盘格式（`.meprefab`）

- JSON，与 `.mescene` / `.memtl` 同族：`Serializer::ToFile` / `FromFile`
- 根对象：`Prefab`
- 含 `$schemaVersion` / `$engineVersion`（CORE-F18）
- Sidecar `.meprefab.meta`：`AssetMeta::Guid` 与加载后 `Prefab::m_Guid` 对齐（现有 AssetManager 惯例）

示意（逻辑形状，非最终字段名）：

```json
{
  "$schemaVersion": 1,
  "$engineVersion": "0.0.9",
  "$type": "Prefab",
  "m_Guid": "...",
  "m_RootGameObject": {
    "$type": "GameObject",
    "m_Guid": "template-root-...",
    "m_Name": "TargetDummy",
    "m_Parent": null,
    "m_Components": [ /* Instanced */ ],
    ...
  }
}
```

子 GO：作为 Instanced 嵌在树上，或通过与 Scene 相同的「根列表 + parent Guid」策略。**推荐与 Scene 一致的序列化形状尽量复用**（Instanced 子对象 + Guid 引用 parent），但 **容器只有一个根**，无 `m_GameObjects` 向量多根。

实现选择（拍板默认）：

| 选项 | 结论 |
|------|------|
| Prefab 内嵌「仅根 + 子树全 Instanced 展开」 | **选用** — 单根清晰 |
| Prefab 内藏 `vector<GO>` 假装小 Scene | **拒绝** |

若子 GO 目前只通过 `m_Parent` Guid 引用、实际所有权在 Scene 的 `m_GameObjects`：Create 时必须 **收集子树并改为 Prefab 所有权模型**（根 `shared_ptr` + 子节点如何被持有）。  
**推荐所有权：** Prefab 只 `shared_ptr` 持有根；子 GO 由父 GO 的 children / 或额外 `ME_PROPERTY(Instanced) vector` 持有 — 需与现有 GO 模型对齐。

**现状缺口：** `GameObject` 的 children 是运行时 `vector<GameObject*>`，序列化靠 Scene 的 flat `m_GameObjects` + `m_Parent`。  
**F23 解决策略（默认）：**

```cpp
// Prefab 内部存储与 Scene 类似的 flat 列表，但语义属于模板：
ME_PROPERTY(Instanced)
std::vector<std::shared_ptr<GameObject>> m_TemplateObjects;

ME_PROPERTY()
GUID m_RootGuid;  // must be one of m_TemplateObjects
```

或仅 `m_RootGameObject` + 序列化时把整棵子树以嵌套 Instanced 写出（若 Serializer 对 parent 边支持）。  
**默认拍板：`m_TemplateObjects` + `m_RootGuid`**，与 Scene flat 列表同构，Create/Instantiate/Resolve 路径对称；对外 API 仍暴露「单根树」。

```cpp
ME_CLASS()
class Prefab : public Asset
{
    ME_GENERATED_BODY()
public:
    GameObject* GetRootGameObject() const;
    const std::vector<std::shared_ptr<GameObject>>& GetTemplateObjects() const;

private:
    ME_PROPERTY(Instanced)
    std::vector<std::shared_ptr<GameObject>> m_TemplateObjects;

    ME_PROPERTY()
    GUID m_RootGuid;
};
```

不变量：`m_RootGuid` 对应列表中 parent==null 的唯一对象；其余 GO parent 均在列表内。

### 3.3 引用切断（Create Prefab）

对模板树上每个反射 `MEObject*` / `shared_ptr<MEObject>` 字段（非 Instanced 所有权边）：

| 目标 | 动作 |
|------|------|
| 指向树内对象（Guid ∈ template set） | **保留** Guid 引用 |
| 指向 `Asset` 派生（或 AssetManager 可解析的资产 Guid） | **保留** |
| 其他（树外 Scene GO/Component、野生对象、零 Guid） | **置空** + 记入 `PrefabCreateReport::BrokenRefs` |

```cpp
struct PrefabBrokenRef
{
    GUID OwnerTemplateGuid;
    std::string PropertyPath;
    GUID PreviousTargetGuid; // if any
    std::string Reason;      // "ExternalNonAsset" / ...
};

struct PrefabCreateReport
{
    std::vector<PrefabBrokenRef> BrokenRefs;
    // warnings only; Create still succeeds unless root invalid
};
```

Instanced 所有权边（`m_Components`、模板列表内对象）**不是** Guid 引用切断对象。

### 3.4 API 契约

命名空间：`minEngine`；实现类建议 `PrefabUtility`（静态方法，无状态）或 `PrefabSystem`（若需服务生命周期）。**默认：`PrefabUtility`。**

```cpp
struct PrefabInstantiateParams
{
    Transform WorldTransform;          // applied to instance root
    GameObject* AttachParent = nullptr; // optional; null = scene root
    bool bRegisterPrefabInstance = true; // Scene editor path: true; bake path: false
};

class PrefabUtility
{
public:
    /** Collect subtree, clone into new Prefab, strip external refs, assign stable template Guids. */
    static std::shared_ptr<Prefab> CreatePrefabFromGameObject(
        GameObject& root,
        PrefabCreateReport* outReport = nullptr);

    /** Write asset to project path via AssetManager (JSON + meta). */
    static bool SavePrefabAsset(
        Prefab& prefab,
        const std::string& projectRelativePath,
        std::string* outError = nullptr);

    /** Load by path or Guid — thin wrappers over AssetManager. */
    static std::shared_ptr<Prefab> LoadPrefabAsset(const GUID& assetGuid);

    /**
     * Clone template tree into target Scene.
     * Remaps template Guids → new instance Guids.
     * Keeps asset Guid refs. Assigns new Scene-local m_ID.
     * If bRegisterPrefabInstance, appends PrefabInstanceRecord with empty Overrides.
     */
    static std::shared_ptr<GameObject> Instantiate(
        const Prefab& prefab,
        Scene& targetScene,
        const PrefabInstantiateParams& params,
        std::string* outError = nullptr);

    /** True if guid is a template object inside this asset. */
    static bool IsTemplateObject(const Prefab& prefab, const GUID& objectGuid);

    /** Find PrefabInstanceRecord for an instance object in a Scene (editor). */
    static PrefabInstanceRecord* FindInstanceRecord(Scene& scene, const GUID& instanceObjectGuid);
};
```

**Runtime≡Editor：** `Instantiate` 可在无 Editor 链路上调用（测试、未来 Gameplay spawn）。Gameplay 调用时 `bRegisterPrefabInstance = false` 即可（或 Scene 非 Editor 时强制 false）。

**Create 后源 Scene 行为（默认拍板）：**

| 选项 | 说明 | 结论 |
|------|------|------|
| A. Create 只写资产，Scene 中原树不动 | 简单 | 可作第一步 |
| B. Create 后将原树登记为该 Prefab 的实例（换 Guid + 写 PrefabInstanceRecord） | 贴近 Unity Apply | **选用为完整 F23 行为** |

B 需要一次「原地转实例」：保持组件数据，Guid remap 到新世界 id，mappings 指向资产内模板 Guid。

### 3.5 数据流

#### Create Prefab

```text
Scene GO root
  → CollectSubtree(root) → set S
  → DeepClone into Prefab.m_TemplateObjects (new template Guids)
  → StripExternalNonAssetRefs(S')
  → Validate single root
  → SavePrefabAsset → .meprefab + .meta
  → (B) Remap original Scene subtree Guids + append PrefabInstanceRecord
```

#### Instantiate

```text
Load Prefab (or in-memory)
  → Binary or reflection clone of m_TemplateObjects with ObjectCloneContext active
  → Remap internal Guid refs via clone map
  → Asset Guid refs unchanged
  → InsertRestoredGameObject for each GO (new m_ID)
  → ResolveGameObjectHierarchy + SC attach + activation
  → Apply root WorldTransform / optional AttachParent
  → Append PrefabInstanceRecord (mappings from clone context; Overrides empty)
```

实现提示：可 `SerializeObjectToBuffer(prefab)` / 仅序列化模板列表，再 `Deserialize` 进 Scene 所有权 — 与 PIE 同构；或手写 Instanced 克隆。**优先复用 Serializer + CloneContext**，避免第三条克隆实现。

#### Scene Load（Editor）

```text
Deserialize Scene
  → GOs + m_PrefabInstances
  → Do NOT strip instance records
  → Hierarchy resolve as today
```

#### PIE / Runtime bake

```text
DuplicateForPIE / load cooked
  → Instantiated GOs already concrete
  → Drop or ignore m_PrefabInstances on PIE Scene
  → No Prefab identity on GO
```

F23：Editor→PIE 全 Scene 克隆会拷贝 `m_PrefabInstances`；PIE Scene 应 **清空** 该表（或 Duplicate 后清除），避免 Play 模式误用编辑链接。

### 3.6 模块与文件布局

```text
Runtime/Function/Framework/Prefab/
  Prefab.h / .cpp
  PrefabTypes.h          // PrefabInstanceRecord, mappings, reports, params
  PrefabUtility.h / .cpp
  PrefabReferencePolicy.h / .cpp   // strip external refs

Runtime/Function/Framework/Scene/
  Scene.h                // + m_PrefabInstances
  SceneCloneContext.*    // or ObjectCloneContext extraction

Runtime/Resource/
  AssetTypeRegistry.cpp  // register Prefab / .meprefab
  AssetPipelineBootstrap.cpp
  Loaders/PrefabLoader.cpp
  AssetManager.*         // Create/Save Prefab specializations as needed

Tests/Suites/PrefabTest.cpp
```

### 3.7 Asset 注册

```cpp
RegisterType({
    .AssetTypeId = "Prefab",
    .RuntimeClassName = /* Prefab::StaticClass() */,
    .Extensions = {".meprefab"},
    .FileDialogFilterLabel = "Prefab (*.meprefab)"
});
```

Content Browser：F23 可只保证 Load/Save/CreateAsset 管线；双击打开 → ED-F16。

### 3.8 与 GameObject / Component 的边界

- **不**修改 `GameObject` 增加 Prefab 字段。
- Component 序列化行为不变；切断逻辑走通用反射扫描。
- `m_ID`（Scene-local）仍不进 Prefab 资产；Instantiate 时由 Scene 分配。

### 3.9 错误处理

| 情况 | 行为 |
|------|------|
| Create 根为 null / 不在 Scene | Fail |
| 子树含第二「逻辑根」 | 不应发生；以 parent 收集为准 |
| Instantiate 时 Prefab 无根 | Fail |
| 外部引用被切 | Success + report warnings |
| 保存路径非法 | Fail + error string |

### 3.10 不变量（汇总）

1. Prefab 单根；`m_RootGuid` ∈ `m_TemplateObjects`；唯一 parent==null。  
2. 模板 Guid 在资产保存间稳定（Create 时生成后不变）。  
3. 实例 Guid ∉ 模板 Guid 集合（同一时刻 ObjectManager 内不共享）。  
4. Prefab 磁盘无树外非资产引用。  
5. F23 下 `Overrides.empty()` 恒成立。  
6. Runtime/PIE GO 无 Prefab API；链接仅 Editor Scene 表。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| Prefab 继承 Scene | 少写容器 | 污染 Scene 语义、RT/Tick/PIE | **拒绝** |
| Instantiate 后断开、不写 PrefabInstance | 更瘦 | F24 必改 Scene 格式 | **拒绝**（F23 写空表） |
| 第二种 VirtualGuid 类型 | 文档清晰 | 双轨身份、Serializer 分叉 | **拒绝**（文档不变量即可） |
| flat `m_TemplateObjects` + `m_RootGuid` | 与 Scene 对称 | 比单 `shared_ptr` 根略冗 | **选用** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| GO 所有权模型与 Scene flat 列表不一致 | Create 丢子节点 | Prefab 采用同构 flat 列表；单测多级子树 |
| 引用扫描漏字段 | 幽灵外部引用 | 反射遍历 + 单测；BrokenRefs 可见 |
| 与 PIE CloneContext 缠死 | Prefab/PIE 回归 | 抽出 `ObjectCloneContext` |
| 手写 `.meprefab` 破坏 Guid | F24 难做 | F18 schema；文档注明勿改模板 Guid |
| Editor 误在 Runtime 依赖 PrefabInstance | 打包行为分叉 | bake 清除；无 GO 字段 |

---

## 6) 验收标准

- [x] `Prefab` 资产类型可 Create / Save / Load（`.meprefab` + meta）
- [x] `CreatePrefabFromGameObject`：多级子树、树内互引保留、资产引用保留、外部引用 null + report
- [x] `Instantiate`：进现有 Scene；新 Guid；层级正确；可选挂 parent；`PrefabInstanceRecord` 写入且 Overrides 空
- [x] Scene 存盘再开：实例链接仍在；GO 数据完整（Create→实例链接；磁盘 roundtrip 覆盖模板）
- [x] PIE：实例可 Tick/渲染为普通 GO；PIE Scene 清空 `m_PrefabInstances`
- [x] `minEngineTests.exe test prefab` 绿
- [x] Core 无 Editor UI 依赖

---

## 7) 建议实现切片

| Slice | 内容 | 验证 |
|-------|------|------|
| **S01** | `Prefab` + 类型注册 + Load/Save | 文件往返 | **Done** |
| **S02** | `ObjectCloneContext` + 子树克隆进 Scene | 单测 Instantiate | **Done** |
| **S03** | Create + 引用切断 + report | 单测 BrokenRefs | **Done** |
| **S04** | Scene `m_PrefabInstances` + Create 原地转实例 + Instantiate 登记 | Scene / Prefab 往返 | **Done** |
| **S05** | PIE 清表 / bake 契约 + DoD | `test prefab` 全绿 | **Done** |

---

## 8) 开放点（全部按默认拍板并已落地）

| # | 问题 | 拍板 | 落地 |
|---|------|------|------|
| O1 | Create 后源树是否变成实例 | **是（B）** | Create 后写 `PrefabInstanceRecord` |
| O2 | Prefab 内部存储 | **`m_TemplateObjects` + `m_RootGuid`** | `Prefab.h` |
| O3 | CloneContext 抽出 | **是，`ObjectCloneContext`** | `Runtime/Core/Object/ObjectCloneContext.*`；PIE 共用 |
| O4 | F23 是否声明空的 `PrefabPropertyOverride` 结构 | **是（稳定 schema）**；字段语义 F24 定义 | `PrefabTypes.h`；F23 恒空 |
| O5 | 最小 Editor 命令 | **可选**：测试/Agent 用 API 即可 | `test prefab` + `PrefabUtility`；右键菜单 → ED-F16 Amendment A |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | Draft：三期拆分之 F23；数据结构 / API / 引用切断 / 空 PrefabInstance |
| 2026-09-15 | 类名定稿：**`Prefab`**（拒绝 `PrefabAsset`）；资产类型 Id 仍为 `"Prefab"`，扩展名 `.meprefab` |
| 2026-09-15 | **Done**：S01–S05 落地；`test prefab` PASS；开放点 O1–O5 按默认全部接受 |
| 2026-09-15 | UTF-8 重写：修复编码损坏；与实现对齐 `m_TemplateObjects` + `m_RootGuid` |
| 2026-09-15 | API note（ED-F16 Amendment A）：计划增加 `Scene::Instantiate` 薄封装转发 `PrefabUtility::Instantiate`（契约不变；详见 ED-F16 §3.10.5） |
