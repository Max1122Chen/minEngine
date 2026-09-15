# ED-F16 — Prefab Editor (Document Mode + SceneEditor Reuse) — Design Spec

## Meta
- **ID:** `ED-F16`
- **Type:** Feature
- **Status:** Done（含 Amendment A：Hierarchy Prefab 工作流 + `Scene::Instantiate`）
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/prefab`
- **Related:**
  - [CORE-F23 Prefab Asset + Instantiate](../Platform/Core/CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md)（**硬依赖**；Amendment A 扩展薄 `Scene::Instantiate`）
  - [CORE-F24 Prefab Overrides](../Platform/Core/CORE-F24_PREFAB_OVERRIDES_DESIGN.md)（Inspector override 标记；Hierarchy 仅做实例身份提示）
  - [ED-F11 Multi-document Tab Host](./ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md)
  - [ED-F13 Command-first](./ED-F13_EDITOR_COMMAND_FIRST_REFACTOR_DESIGN.md)
  - [ENGINE_0_1_0_ROADMAP](../ENGINE_0_1_0_ROADMAP.md) — Prefab B / 隔离 RT 说明
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
- **Depends on:** CORE-F23 **Done**（类名 `Prefab`）；ED-F11 文档宿主；现有 `SceneEditor` 内核；ContextMenu（Hierarchy actions）
- **Blocks:** 舒适的 Prefab 创作流（无 Hierarchy Create 则无法在编辑器内产出 `.meprefab`）

## TL;DR

用 **独立文档 Tab** 编辑 Prefab（像 UE 的 Level Tab vs Blueprint/Actor Tab），但 **复用同一套 SceneEditor 底层**（像 Unity Prefab Mode）：按 **编辑上下文** 开放/限制功能。依赖 F23 的 `Prefab` 资产（非 PrefabAsset）。

- Prefab 文档背后是 **临时 Stage Scene**：**不可另存为 `.mescene`**；Save 写回 **`.meprefab`**。
- Hierarchy **有且仅有一个根 GO**。
- MVP：**聚焦占用主 Viewport**（不与 Level 并排实时预览）；隔离 RT / 串图问题 **不进本 Feature DoD**。
- **Amendment A：** Level Hierarchy 右键 **Create Prefab** / **Instantiate Prefab**；实例与普通 GO 视觉区分；Core `Scene::Instantiate` 薄封装（Unity 式 `scene.Instantiate(prefab)`）。

## Scope

### In（ED-F16 原 MVP — 已落地 S01–S06）

- `EditorDocumentTypeId = "Prefab"` 注册到 ED-F11 Host
- Prefab 文档 Session：资产身份、Dirty、自有 CommandStack
- Stage Scene：从 `Prefab` 实例化编辑用树（编辑期可保留模板 Guid 或使用编辑映射 — §3.2）
- 复用 SceneEditor 能力：Hierarchy / Viewport / Inspector / 选择 / 大部分 Scene 命令
- **上下文限权：** 禁止另存 Scene、禁止第二顶层根、Save→Prefab、标题/Tab 显示 Prefab 名
- Open：Content Browser 双击 `.meprefab` → `OpenOrFocus` Prefab 文档
- 退出 / 关 Tab：确认 Dirty；丢弃 Stage Scene

### In（Amendment A — Hierarchy Prefab 工作流）

| 项 | 说明 |
|----|------|
| Hierarchy → **Create Prefab…** | 右键选中 GO（子树根）→ 另存 `.meprefab` → `CreatePrefabFromGameObject` + `SavePrefabAsset`；保留 F23 的实例链接（空 override 表） |
| Hierarchy → **Instantiate Prefab…** | 空白处或 GO 上右键 → 选资产 → 写入当前 Level Scene；可选「作为选中对象的子节点」 |
| Hierarchy **实例外观** | Prefab 实例根（及默认含其子孙）与普通 GO 区分：图标 + 着色（Unity 式蓝系）；**不做** override 蓝字明细 |
| **`Scene::Instantiate`** | Runtime 薄封装：`scene.Instantiate(prefab, params)` → 转发 `PrefabUtility::Instantiate`；单测覆盖 |

### Out

| 项 | 归属 |
|----|------|
| Level 与 Prefab **并排**同时渲染 | 隔离 RT（`RND-F17` 类）；本 Feature **不做** |
| 完整 override 可视化（Inspector 蓝字） | F24 机制 + 后续 Editor UX；Hierarchy 仅身份区分 |
| Nested Prefab 钻入 / 面包屑 | 后置 |
| Prefab Variant | 后置 |
| 缩略图管线修复（TD-027） | 不绑本 Feature |
| 从 Content Browser **拖拽** Prefab 进 Hierarchy / Viewport | Amendment A **Out**（可后置；先右键菜单） |
| Prefab Stage 内再 Create Prefab（嵌套资产） | Amendment A **Out**（仅 Level Scene 上下文） |
| Apply to Prefab（实例写回资产） | 仍 O5 / 后置 |

## Reader quick start

1. §3.1 产品模型 · §3.2 Stage Scene · §3.3 上下文限权 · §3.4 数据流 · §3.5 与 SceneEditor 集成  
2. **§3.10 Amendment A**（Hierarchy Create/Instantiate、实例外观、`Scene::Instantiate`）  
3. §5 风险 · §6 验收 · §7 切片  

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 依赖 | F23 Prefab 资产 **sound（须先 Done）**；ED-F11 Host **sound**；SceneEditor **sound**；双视口 RT **missing** |
| 债风险 | **Medium** — 若复制一套 PrefabEditor，长期分叉；若承诺并排预览，被串图拖死 |
| WIP | 勿阻塞 F23；Editor 可排在 F23 之后 |
| 哲学 | 编辑器是机制复用 + 上下文策略；非第二套世界观 |
| 建议 | **Go with scope cut** — 单 Viewport Mode + Stage Scene + 限权；并排预览 Out |

---

## 1) 背景与目标

### 1.1 Pain

- F23 允许手写 `.meprefab`，但不可视编辑。
- 维护者希望：**文档模型像 UE（多 Tab）**，**实现像 Unity（复用 Scene 编辑内核）**。

### 1.2 Goals

1. Prefab 作为一等文档打开，与 Scene 文档并列（Host 层）。
2. 不 fork 全套编辑器：同一 SceneEditor / 命令面，按 `EditorEditContext` 限权。
3. Stage Scene 明确「不可保存为关卡」；用户 Save = 保存 Prefab 资产。
4. 单根 Hierarchy 不变量由 UI + 命令校验共同保证。

### 1.3 Success

- 双击 Prefab → Tab 打开 → 改组件属性 → Save → 磁盘 `.meprefab` 更新；无 `.mescene` 写出。
- 尝试添加第二顶层 GO → 被拒绝。
- 切回 Scene Tab → Stage 不泄漏进关卡文档。

---

## 2) 现状

- `SceneEditor`：打开 `.mescene`、Hierarchy、Viewport、Inspector、命令面（ED-F13/F14）。
- `EditorDocumentHost`：多 Session / Tab（ED-F11）。
- 无 Prefab 文档类型；无 Stage Scene 生命周期。
- 已知：单 ForwardRenderer + 单 RDG 多同尺寸 RT **串图**（Roadmap Prefab B / TD-027）。

---

## 3) 方案

### 3.1 产品模型

```text
EditorDocumentHost
  ├─ Session "Scene"     → document Scene (persistent .mescene)
  ├─ Session "Prefab"    → Prefab + Stage Scene (transient)
  └─ Session "Material"  → …
```

用户心智：

| | Scene 文档 | Prefab 文档 |
|--|------------|-------------|
| 编辑对象 | 关卡 | Prefab 资产 |
| 视口内容 | Editor Scene | Stage Scene（仅该 Prefab 根树） |
| Save | 写 `.mescene` | 写 `.meprefab` |
| 另存为 Scene | 允许（现有） | **禁止** |
| 顶层 GO 数量 | 多个 | **恰好 1** |

### 3.2 Stage Scene

```cpp
struct PrefabEditorStage
{
    std::shared_ptr<Prefab> Asset;          // document truth for Save
    std::shared_ptr<Scene> StageScene;           // ESceneType::Editor; not in project asset registry as savable scene
    // Optional: map templateGuid ↔ stage instanceGuid if stage uses instance Guids
    ObjectCloneContext EditCloneMap;
    bool bOwnedByPrefabSession = true;
};
```

**创建 Stage：**

```text
Open Prefab
  → Create empty Scene (not registered as project Scene asset)
  → PrefabUtility::Instantiate(asset, stageScene, {
        bRegisterPrefabInstance = false,  // editing the asset itself, not a level instance
        WorldTransform = Identity
     })
  → OR: load template objects directly into stage with stable template Guids
```

**两种 Guid 策略：**

| 策略 | 说明 | 结论 |
|------|------|------|
| **A. Stage 使用模板 Guid** | Save 时直接把 Stage 树写回 Prefab；简单 | 与 ObjectManager 全局唯一紧张；同时打开的 Scene 若仍引用同 Guid 会炸 |
| **B. Stage 使用编辑用实例 Guid，Save 时 remap 回模板 Guid** | 安全 | 多一步 | 

**默认：B**。打开时 Instantiate 到 Stage（新 Guid）；Save 时用 `EditCloneMap` **反向**写回 `Prefab.m_TemplateObjects`，**保持原模板 Guid**（按 mapping 左端），避免 F24 键失效。

若 Mapping 丢失 → Save Fail（不可静默生成全套新模板 Guid，除非用户确认「破坏性重建」—— MVP 直接 Fail）。

**Stage Scene 标志：**

- `ESceneType::Editor`
- 不进入「项目 Scene 列表 / 关卡流」
- `CanSaveAsScene() == false`
- TickPolicy：`ViewportOnly` 或与编辑 Scene 相同但不跑 Gameplay 全局 — 默认 `ViewportOnly`

### 3.3 编辑上下文（限权核心）

```cpp
enum class EEditorWorldContext : uint8_t
{
    LevelScene,
    PrefabStage,
    // future: AnimationPreview, ...
};

struct EditorEditContext
{
    EEditorWorldContext WorldContext = EEditorWorldContext::LevelScene;
    Scene* ActiveScene = nullptr;           // Level or Stage
    Prefab* ActivePrefab = nullptr; // non-null iff PrefabStage
    PrefabEditorStage* Stage = nullptr;
};
```

`IEditorContext` / 活跃文档 Session 提供 `GetEditContext()`。  
`SceneEditor` 不分裂成两个类；命令与 UI 查询 context：

| 能力 | LevelScene | PrefabStage |
|------|------------|-------------|
| Hierarchy 多根 | 是 | **否**（禁 Create 顶层第二 GO） |
| 子 GO 增删（单根下） | 是 | 是 |
| Save | Save Scene | **Save Prefab** |
| Save As `.mescene` | 是 | **否** |
| Play / PIE | 是 | **否**（或后置「仅 Stage PIE」；MVP 否） |
| 拖入 Prefab 再嵌套 | 是（F23 实例） | MVP **否** |
| Override 标记 | 有 F24 时显示 | **无**（改的是 default） |
| 删除根 GO | 视现有 | **否** |

### 3.4 数据流

#### Open

```text
TryOpenAsset(.meprefab)
  → DocumentTypeRegistry finds "Prefab"
  → Create PrefabDocumentSession
  → Load Prefab
  → Build Stage (Instantiate B)
  → OpenOrFocus Tab
  → OnActivate: SceneEditor binds EditContext=PrefabStage, ActiveScene=Stage
```

#### Edit

```text
User edits component on stage GO
  → SceneEditor commands / AssignProperty
  → Mark PrefabSession Dirty (not Level Scene)
  → Stage Scene dirty flag local only
```

#### Save

```text
User Save (Ctrl+S)
  → PrefabDocumentSession::Save
  → Validate single root
  → Write Stage tree → Prefab templates (preserve template Guids via map)
  → PrefabUtility::SavePrefab
  → If F24 present: PrefabOverrideUtility::PropagateDefaultsToOpenScenes(*asset)
  → Clear Dirty
```

#### Close / Switch away

```text
OnDeactivate Prefab Tab
  → SceneEditor rebinds to Level Scene context (or next active doc)
  → Stage Scene 可保留在 Session 内直到 Close

OnClose
  → Prompt if Dirty
  → Destroy Stage Scene / unregister objects
```

### 3.5 与 SceneEditor / Host 集成

**推荐结构：**

```text
Editor/
  SubEditor/Scene/SceneEditor.*     // shared kernel; context-aware
  SubEditor/Prefab/
    PrefabDocumentSession.*         // ED-F11 session impl
    PrefabStageController.*         // build/save/teardown stage
    PrefabEditConstraints.*         // validators used by commands
```

**不推荐：** `PrefabEditor : SceneEditor` 大复制。  
**可以：** `PrefabDocumentSession` 在 Activate 时调用 `SceneEditor::EnterPrefabStage(stage)` / `ExitPrefabStage()`。

命令注册（ED-F13）：

- 复用大部分 `Editor*GameObject*` / component 命令
- 包装一层：若 `WorldContext==PrefabStage`，走 `PrefabEditConstraints`
- 新增：`EditorSavePrefabDocument`（或通用 Save 路由按 TypeId 分发）

Tab 标题：`PrefabName*`（Dirty 星号）；图标可与 Scene 区分。

### 3.6 Viewport / 渲染（MVP）

```text
Active document is Prefab
  → Main ViewportClient renders Stage Scene only
Active document is Scene
  → Main ViewportClient renders Level Scene
```

**不做：** 同一帧两个 Scene 各画一块 RT。  
切换 Tab = 切换渲染源。这满足「像 UE 的多 Tab」，暂不承诺「像 Unity 的嵌套 Prefab 预览窗」。

隔离 RT 就绪后，可另开 Feature 增强为并排 / 内嵌预览，**不改** Prefab 资产契约。

### 3.7 单根不变量

强制点：

1. Stage 创建后只有 Instantiate 的一根。  
2. `CreateGameObject` 命令：若 PrefabStage 且无选中父 GO → **Fail**（不允许新顶层）；若选中树内 GO → 作为子节点创建。  
3. Unparent 到「无父」：若对象不是当前根 → **Fail**（避免变第二根）；根 Unparent 无意义。  
4. Save 前 `ValidatePrefabStage(stage)`：恰好一个 parent==null。

### 3.8 Dirty / Undo

- Prefab Session **自有 CommandStack**（ED-F11 模型）。
- Undo 不跨 Level / Prefab Session。
- Level Scene 的 Prefab **实例**编辑（F24 override）留在 **Scene Session** 栈。

### 3.9 与 F23/F24 的关系

| | F23 | F24 | F16 |
|--|-----|-----|-----|
| 手写改 Prefab | 是 | — | 被可视化替代 |
| Scene 里 Instantiate | 是 | override | 打开 Prefab 文档无关 |
| Save Prefab 触发传播 | — | API | Save 调用 |

F16 **不** 在 Stage 上写 `PrefabInstanceRecord`（改的是资产 default）。

### 3.10 Amendment A — Hierarchy Prefab 工作流 + `Scene::Instantiate`

> **状态：Done（已实现）。**  
> 动机：S01–S06 打通了「打开已有 Prefab 文档」，但 **无法在 Editor 内从 Level GO 创建 `.meprefab`**，也无法从 Hierarchy 放入实例；API 调用面仍偏 `PrefabUtility::Instantiate(prefab, scene)`。

#### 3.10.1 产品行为（Level Scene 上下文）

| 入口 | 行为 |
|------|------|
| 右键 **GameObject** → `Create Prefab…` | 以该 GO 为根（含子树）创建；弹出保存路径（项目 Content 下 `.meprefab`）；成功后磁盘有资产，且源 GO 成为带空 override 的 Prefab 实例（沿用 F23 `CreatePrefabFromGameObject` 链接语义） |
| 右键 **空白** → `Instantiate Prefab…` | 资产选择器（过滤 `AssetType==Prefab`）→ 实例化到当前 Level，**顶层**放置（Identity 或选中附近可选后置） |
| 右键 **GameObject** → `Instantiate Prefab…` | 同上；默认仍顶层。子菜单或同对话框勾选 **Instantiate as Child**：`AttachParent = selection` |
| Prefab **Stage** Hierarchy | **不**提供 Create Prefab / Instantiate Prefab（避免嵌套资产与 Stage 语义混乱）；仅 Level 文档 |

实现落点：现有 `EditorContextMenuSystem` + `HierarchyMenuContext`（与 `Create Empty` / Duplicate 同轨），新增 `EditorActionId`，**不要**在 `HierarchyWindow` 内硬编码整段逻辑。

#### 3.10.2 Create Prefab 数据流

```text
Hierarchy GO context menu → Create Prefab…
  → (optional) validate single connected subtree / has Root SceneComponent
  → FileDialog SaveAs *.meprefab under ProjectContentRoot
  → PrefabUtility::CreatePrefabFromGameObject(*go)   // already registers PrefabInstanceRecord
  → PrefabUtility::SavePrefabAsset(*prefab, relativePath)
  → Mark Scene dirty；选中仍留在实例根
  → 默认不自动打开 Prefab 文档 Tab（减少打断；用户可再双击 CB）
```

失败：路径非法 / 序列化失败 → 明确日志 + 不留下半截资产（或删半成品 — 实现时 Fail closed）。

#### 3.10.3 Instantiate Prefab 数据流

```text
Instantiate Prefab… → pick AssetMeta(Prefab)
  → LoadAsset<Prefab>
  → scene.Instantiate(*prefab, params)   // §3.10.5
       params.bRegisterPrefabInstance = true（Level Editor Scene）
       params.AttachParent = selection or null
  → Select new instance root；Mark Scene dirty
```

#### 3.10.4 Hierarchy 实例视觉（Unity 式区分）

| 对象 | 外观（默认拍板） |
|------|------------------|
| Prefab **实例根**（`PrefabUtility::FindInstanceRecord` 命中且 `RootInstanceGuid == go`） | Prefab 图标（如 `ICON_FA_CUBES`）+ **实例色**文字（语义色，建议青/蓝系，进 `EditorAppearance` SemanticColors，如 `HierarchyPrefabInstance`） |
| 实例 **子孙** GO | 同色文字（轻量）；图标可仍用普通 GO 或小立方 |
| 普通 GO | 现状不变 |
| Prefab **Stage** 内 GO | **不**用实例色（编辑的是资产 default，不是关卡实例）；可选极弱「Asset」提示，非 DoD |

查询：`SceneEditor` / Hierarchy 绘制时读当前 `GetActiveScene()->GetPrefabInstances()`，对每个节点 O(实例数) 或建 `InstanceGuid→Record*` 临时 map（每帧 Hierarchy 可接受；节点多再缓存）。

**不做：** override 属性蓝字、断开/Unpack 图标矩阵、嵌套 Prefab 多层色。

#### 3.10.5 Core：`Scene::Instantiate`（Unity 式入口）

`PrefabUtility::Instantiate(prefab, scene, …)` 保留为权威实现。  
Scene 增加成员，把 **自身** 填入目标 Scene 参数，降低调用摩擦：

```cpp
// Scene.h — thin convenience; no second clone path
std::shared_ptr<GameObject> Instantiate(
    const Prefab& prefab,
    const PrefabInstantiateParams& params = {},
    std::string* outError = nullptr);

// Implementation:
//   return PrefabUtility::Instantiate(prefab, *this, params, outError);
```

可选（同切片或紧随）：`Instantiate(const Prefab&, const Transform& worldTransform, …)` 仅填 `params.WorldTransform`。

原则（ENGINE_DESIGN_PHILOSOPHY）：

- **Mechanism：** Scene 暴露与 Utility 等价的机制入口；不引入 Gameplay PrefabManager。  
- **单一路径：** 禁止 Scene 内复制一套 clone 逻辑。  
- **测试：** `test prefab` 增加 `scene->Instantiate(*prefab)` 冒烟（与现有 Utility 路径对等）。

文档归属：行为契约仍以 F23 为准；本 Amendment 在 F16 描述 Editor 消费，并在 F23 变更记录挂一条「API 便利封装」。

#### 3.10.6 与 Stage / F24 边界

| 场景 | Create Prefab | Instantiate Prefab | 实例着色 |
|------|---------------|--------------------|----------|
| Level Scene 文档 | 是 | 是 | 是 |
| Prefab Stage 文档 | 否 | 否 | 否（非实例） |
| PIE | 否 | 否（或后置） | N/A |

Save Prefab 文档后的 Propagate（已有）不变。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| 独立 PrefabEditor 全复制 | 快叉开 | 双倍命令债 | **拒绝** |
| 并排双 Viewport MVP | 体验好 | 串图 / 隔离 RT | **拒绝（本期）** |
| Stage 使用模板 Guid（策略 A） | Save 简单 | Guid 冲突 | **拒绝** |
| Prefab 只能在外部 DCC 编辑 | 零编辑器成本 | 违背目标 | **拒绝** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| SceneEditor 全局单例状态泄漏 | 改 Prefab 脏了 Level | Session 激活时显式绑定 context；单测切换 Tab |
| Save 弄丢模板 Guid | 打断 F24 | 反向 mapping；Fail closed |
| 用户以为 Stage 能 Save As Scene | 资产污染 | UI 禁用 + 文案「Saving Prefab」 |
| 过早承诺并排预览 | 排期被 RT 拖死 | DoD 写明单 Viewport |
| PIE 误用 Stage | 怪异 | PrefabStage 禁用 Play |
| Hierarchy Create 与 Stage 混淆 | 嵌套 Prefab 半成品 | Stage 隐藏 Create/Instantiate |
| 实例着色误伤 Stage GO | 编辑资产时满屏蓝 | Stage 禁用实例色 |
| Scene::Instantiate 分叉实现 | 双路径 bug | 仅转发 PrefabUtility |

---

## 6) 验收标准

### 6.1 原 MVP（S01–S06）— 已完成

- [x] CB 双击 `.meprefab` → Prefab Tab；与 Scene Tab 可切换（DocumentType Prefab + OpenOrFocus）
- [x] 编辑 Stage → Dirty → Save → WriteStageTreeToPrefab（Guid 保持）+ SavePrefabAsset；单测 writeback
- [x] 无法 Save As `.mescene`；无法创建第二顶层 GO（无选中父时）；无法删除根；禁 PIE
- [x] 关 Tab：CloseSession Discard Stage；InspectingScene 回 Level
- [x] 切回 Scene Tab：ExitPrefabStage → Viewport 显示关卡
- [x] Save Prefab 调用 `PropagateDefaultsToOpenScenes`（F24）
- [x] 无第二套 Hierarchy/Inspector（复用 SceneEditor ModuleId）

### 6.2 Amendment A（S07–S11）

- [x] Level Hierarchy 右键 GO → Create Prefab… → 磁盘 `.meprefab` + 源 GO 成为实例链接
- [x] Level Hierarchy 右键空白/GO → Instantiate Prefab… → 场景出现实例根；可选 as Child
- [x] Prefab Stage Hierarchy **无**上述两项菜单（或始终 Disabled + 原因）
- [x] Hierarchy 中 Prefab 实例根（及子孙）与普通 GO 视觉可区分
- [x] `Scene::Instantiate` 存在且与 `PrefabUtility::Instantiate` 行为一致；`test prefab` 覆盖
- [x] 命令走 ContextMenu / Command 栈；Create/Instantiate 可 Undo（至少 Instantiate / 删除实例；Create 的资产文件 Undo **Out** — 仅撤销场景侧链接可选，默认拍板：场景 dirty + 命令撤销实例链接困难则 **Instantiate Undo = 删实例根**；Create Prefab **不**自动删磁盘文件）

---

## 7) 建议实现切片

### 7.1 原 MVP

| Slice | 内容 | 验证 |
|-------|------|------|
| **S01** | Prefab DocumentType + Session 壳 + OpenOrFocus | Tab 可开空壳 |
| **S02** | Stage 构建 / 销毁 + Viewport 绑定 | 能看见 Prefab 根 |
| **S03** | Save 回写资产（Guid 保持） | 磁盘往返 |
| **S04** | EditConstraints（单根 / 禁 SaveAs Scene / 禁 PIE） | 负向用例 |
| **S05** | Dirty / Undo 栈隔离；与 F24 Propagate 挂钩 | Host Session Dirty + per-session CommandStack；Save→Propagate |
| **S06** | DoD | — |

### 7.2 Amendment A

| Slice | 内容 | 验证 |
|-------|------|------|
| **S07** | `Scene::Instantiate` 薄封装 + `test prefab` | 单测 |
| **S08** | Hierarchy `Create Prefab…`（Level only）+ Save 对话框 | 手工 + 可选冒烟 |
| **S09** | Hierarchy `Instantiate Prefab…`（顶层 / as Child） | 手工 |
| **S10** | Hierarchy Prefab 实例着色 + 图标 | 手工 |
| **S11** | Amendment DoD / 文档勾选 | — |

---

## 8) 开放点（设计默认已拍板）

| # | 问题 | 默认 |
|---|------|------|
| O1 | Stage Guid 策略 | **B（编辑实例 Guid + Save 反向 remap）** |
| O2 | Prefab Tab 是否允许 PIE | **否（MVP）** |
| O3 | 并排预览 | **Out** → 隔离 RT Feature |
| O4 | Hierarchy Prefab 实例视觉 | **Amendment A In**（实例色 + 图标；非 override 蓝字） |
| O5 | Apply to Prefab（从 Level 实例） | 非本 Feature 首期；后置 |
| O6 | Create Prefab 后是否自动打开 Prefab Tab | **否** |
| O7 | Instantiate 默认父节点 | **顶层**；可选 as Child of selection |
| O8 | Create Prefab 失败时磁盘残留 | **Fail closed**（尽量不留半成品） |
| O9 | CB 拖拽 Prefab 到 Hierarchy | **Out**（Amendment A） |
| O10 | Create Prefab 的磁盘文件 Undo | **Out**（不删文件）；场景侧以 dirty / 实例链接为准 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | Draft：UE 式文档 Tab + Unity 式 SceneEditor 复用；Stage Scene；单 Viewport |
| 2026-09-15 | 对齐 F23 类名 `Prefab`；Guid 策略 B；UTF-8 重写修复编码损坏 |
| 2026-09-15 | **In Progress：** S01/S02 — Prefab DocumentType + Stage Instantiate + InspectingScene 绑定；Save 回写待 S03 |
| 2026-09-15 | S03/S04：WriteStageTreeToPrefab + Save/Propagate；PrefabEditConstraints（单根/禁 SaveAs/禁 PIE） |
| 2026-09-15 | **Done：** S01–S06；EnterPlay 硬拦 Prefab Stage；DoD 勾选 |
| 2026-09-15 | **Amendment A → Review：** Hierarchy Create/Instantiate Prefab；实例视觉区分；`Scene::Instantiate`；S07–S11（**待审批**） |
| 2026-09-15 | **Amendment A Done：** S07–S11；`Scene::Instantiate`；Hierarchy Create/Instantiate（Level only）；实例色+图标；`test prefab` 8/8 |
