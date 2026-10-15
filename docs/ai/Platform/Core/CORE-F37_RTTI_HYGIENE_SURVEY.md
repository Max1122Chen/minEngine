# CORE-F37 RTTI Hygiene Survey

## Meta
- **ID:** `CORE-F37`
- **Type:** Survey / campaign umbrella（非单一切片实现案）
- **Complexity:** L2（跨 Material / Core / RHI / Editor；子项再拆 Design）
- **Status:** `Review`（维护者审阅 + 对需设计项给出方案；**未授权大规模改码**）
- **Owner:** project maintainer
- **Last updated:** 2026-10-15
- **Related:** [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ObjectCast.h](../../../../minEngine/minEngine/src/Runtime/Core/Object/ObjectCast.h) · Material pin 相关见簇 A1
- **Trust:** Tier A while Status is Review / In Progress — 驱动讨论与后续 Feature 登记；**不要**把本文件 checkbox 当强制 TODO

## TL;DR

盘点仓库内 `dynamic_cast` / `dynamic_pointer_cast`（excl. Third-Party）：约 **68 文件 / 218 处**。  
分成三类：**A 设计错（应消灭闭世界 cast）**、**B 偷懒（MEObject 应用 `Cast<>`）**、**C 诚实边界（暂留）**。  
§7 为 **Agent 推荐方案（待你批准）**；未批准前不写码。禁止用 `Cast<>` 梯子替换 A 类全知函数。

---

## 1) 分类定义（审阅时请沿用）

| 类 | 名称 | 判据 | 合法修法 |
|----|------|------|----------|
| **A** | DesignSmell | 某函数/模块闭世界认识所有子类；或已有更合适机制却仍靠 C++ RTTI 做分派 | 依赖倒置 / 数据声明 / Kind→typed accessor；**不是** Cast 梯子 |
| **B** | LazyRTTI | 对象已在 `MEObject` 反射图上；责任边界正确，仅误用 `dynamic_cast` | 换 `Cast<T>` / `IsA`（见 `ObjectCast.h`） |
| **C** | Legitimate / Defer | 抽象→后端、非 MEObject 边界；或短期无更好 API | 保留并写明理由；可选长期 typed handle |

**红线：** A → B 的“假重构”（全知 `dynamic_cast` 改成全知 `Cast<>`）一律拒绝。

---

## 2) 规模快照（2026-10-15）

| 区域 | 约计 | 备注 |
|------|------|------|
| Material pin / NodeDef 探测 | ~50+ | `MaterialValueType.cpp` 独占 ~40 |
| OpenGL + Vulkan RHI | ~26 | 后端具体类型 |
| Editor 子模块查找 | ~15+ | `EditorContextHelpers` / DocumentTypes |
| Prefab / Scene / GameObject / Component | ~20+ | 多为 MEObject Outer / 组件 |
| Asset loaders / AssetManager | ~15+ | Load 返回基类再转 |
| MIR debug / emitter / GLSL translator | ~12+ | 已有 `MIRValueKind` |
| 其他 Editor / Tests | 余量 | 随 B 机械波 |

Top 文件（按处数）：

1. `MaterialValueType.cpp` — 40  
2. `VulkanRHIResources.cpp` / `EditorAutomationSurface.cpp` / `EditorContextHelpers.h` — 10  
3. `OpenGLRHI.cpp` / `VulkanRHI.cpp` / `MIRDebug.cpp` — 8  

---

## 3) 簇清单（维护者审阅入口）

对每个 **NeedsDesign** 簇：请在「维护者方案」填空，或注明 Deferred。  
**Mechanical** 簇可不写方案，仅确认是否允许开机械切片。

### 簇 A1 — Material NodeDef pin 类型全知函数 — **NeedsDesign**

| | |
|--|--|
| **证据** | `MaterialValueTypeUtil::GetNodeOutputPinType` / `GetNodeInputPinType`：对每个 `MaterialGraphNodeDef_*` 做 `dynamic_cast` 梯子 |
| **为何是 A** | pin 类型知识应属 NodeDef（或 pin 数据）；Util 闭世界，新节点必改中央函数 |
| **连带** | `Material.cpp`、`MaterialTestGraph.cpp` 等对 TextureObject / ScalarParameter / TextureSample 的同类探测 |
| **已有资产** | `MaterialGraphNodeDef` : `MEObject` + `ME_CLASS`；`BuildIR` 已在子类；Input/Output 现仅有 `Name` |
| **开放问题** | ① pin 类型放 **Output/Input 数据字段**（ctor 写入）还是 **虚函数**？② 多态节点（Multiply/Select）如何表达“推断 / nullptr”？③ Util 是否保留为薄转发？④ 与 Editor 连线校验、IR 的关系？ |
| **Agent 推荐** | 见 **§7.1** |
| **维护者裁定** | _（待填：Approve / Amend / Reject）_ |
| **建议后续** | 独立 Feature（候选 `MAT-F07`）+ 薄 L2 Design；验收：`material-ir` + 删梯子 |

### 簇 A2 — MIR 已有 Kind 仍用 `dynamic_cast` — **NeedsDesign（轻）或 Mechanical-if-policy**

| | |
|--|--|
| **证据** | `MIRDebug.cpp`、`MIREmitter.cpp`、`GLSLMaterialTranslatorImpl.cpp`：对 `MIRConstant` / `MIROperator` / … `dynamic_cast` |
| **为何偏 A** | `MIRValue` 已有 `MIRValueKind` + `IsA(Kind)` + 部分 `AsDimensional`；应用 Kind 守卫 + `static_cast`，不必 C++ RTTI |
| **开放问题** | ① 是否统一补齐 `AsConstant` / `AsOperator` / … 助手？② Debug 路径是否允许保留 dynamic_cast（性能无关）？ |
| **Agent 推荐** | 见 **§7.2** |
| **维护者裁定** | _（待填）_ |
| **建议后续** | 政策拍板后小切片；勿与 A1 绑同一 PR |

### 簇 B1 — MEObject / Asset 图 LazyRTTI — **Mechanical（可批）**

| | |
|--|--|
| **证据** | Loaders（Material/Scene/Prefab/Sprite/Texture/Anim…）、`AssetManager`、`Prefab*`、`SceneManager`、`GameObject`/`Component` Outer、`Serializer`、部分 Tests |
| **模式** | `dynamic_cast<T*>(meObject)` / `std::dynamic_pointer_cast<T>(sp)`，T : MEObject |
| **修法** | `#include ObjectCast.h` → `Cast<T>(...)` |
| **风险** | 低；需确认无非 MEObject 误用 |
| **Agent 推荐** | 见 **§7.3** |
| **维护者裁定** | _（待填）_ |

### 簇 B2 — Editor 子模块 id→typed — **Deferred（本战役放过）**

| | |
|--|--|
| **证据** | `EditorContextHelpers.h`、`EditorDocumentTypes.cpp`：`FindSubModule(id)` 返回基类后再 `dynamic_cast<SceneEditor*>` 等 |
| **为何偏 B** | id 已选定具体模块；cast 多为确认。非闭世界全知表 |
| **本战役** | **放过**（无法干净消灭且收益低）；见 **§7.4** |
| **维护者裁定** | Deferred（2026-10-15） |

### 簇 C1 — RHI 后端 downcast — **Legitimate（暂留）**

| | |
|--|--|
| **证据** | `OpenGLRHI.cpp`、`VulkanRHI.cpp`、`VulkanRHIResources.cpp`：`RHI*` → `OpenGL*` / `Vulkan*` |
| **为何 C** | RHI 资源非 MEObject；当前抽象即“后端实现类” |
| **长期可选** | typed handle / CRTP / 后端私有接口；**非本战役必做** |
| **Agent 推荐** | 见 **§7.5** |
| **维护者裁定** | _（待填）_ |

### 簇 C2 — 其他 / 待你点名

| 文件簇 | 初判 | 备注 |
|--------|------|------|
| `EditorAutomationSurface` / Thumbnail / AnimGraph UI | 多为 B 或 Editor 胶水 | 随 B1/B2 扫 |
| `RHICommandList` / `RenderGraph` / `EnvironmentMap` | 个别 | 打开时再归类 |
| `ObjectManager.h` | 查一下是否 MEObject | 机械波时核对 |

---

## 4) 明确不在本战役

- 引入第二套 RTTI / “Agent Framework”
- 为消灭 cast 而强制所有 RHI 类型进 `MEObject`
- 与 CORE-F33 / ASSET-F04 / RND-F24 产品切片混 PR

---

## 5) 建议审阅顺序

1. 确认 §1 三类定义是否同意。  
2. 审 §7.1（A1）— 是否 Approve / 改一刀。  
3. 审 §7.2–§7.5 其余推荐。  
4. 在各簇「维护者裁定」写 Approve/Amend/Reject；需要 L2 Design 的再拆 `*_DESIGN.md`。

---

## 6) 工作区备忘（实现前）

| 项 | 状态 |
|----|------|
| 反射 `Cast<>` 已存在 | 是 — `ObjectCast.h` |
| 注释 ID 卫生（WF-F05） | 已合入 master |
| 本 Survey | Review — §7 待裁定 |
| 实现 | **Blocked until §7 裁定** |

---

## 7) Agent 推荐方案（待批准 · 2026-10-15）

> 以下是 partner 默认路径，不是定稿。你改一刀或整段 Reject 即可。批准前不开码。

### 7.1 簇 A1 — Material pin 类型（推荐默认）

**目标契约**

- 每个 NodeDef **自己声明**固定 pin 的 value type；中央 Util **不再**认识任何具体 `MaterialGraphNodeDef_*`。
- `nullptr` 语义保持现状：`AreConnectable` 把「未知 / 推断」当宽类型（两端都已知才硬比）。

**推荐形态：数据优先，几乎不要虚函数**

1. 扩展 pin 结构（非序列化 / 或 Transient）：
   - `MaterialGraphNodeDefOutput` 增加 `MaterialValueType DeclaredType = nullptr;`
   - `MaterialGraphNodeDefInput` 增加 `MaterialValueType ExpectedType = nullptr;`
2. 各子类 **ctor** 在 `push_back` Name 的同时写入类型（与今日 `BuildIR`/梯子语义对齐），例如：
   - Constant：Output0 = Float  
   - Constant3：Out0 = Float3，Out1–3 = Float  
   - MakeFloat2：In = Float，Out = Float2  
   - TextureSample：In Texture=Texture2D，UV=Float2；Out RGBA/RGB/A 对应 Float4/3/1  
   - Multiply / Add / Select / …：**两侧 ExpectedType / DeclaredType 保持 `nullptr`**（推断）
3. `MaterialValueTypeUtil::GetNode*PinType` 退化为：
   - 边界检查 → 读对应 pin 的字段 → 返回  
   - **删除全部 `dynamic_cast` 梯子**
4. MaterialOutput 输入：继续复用现有 `GetMaterialPropertyInputDescription`（按 input Name 查 ExpectedType），在 **MaterialOutput 的 ctor / EnsureInputs** 里写入各 shading pin 的 ExpectedType，而不是在 Util 里 `dynamic_cast` MaterialOutput。

**为何不首选「每节点虚函数 GetOutputPinType」**

- 固定类型与 Name 同生同灭，数据更贴 ctor 真源；虚函数容易再长出第二份 switch。
- 仅当日后出现「实例状态改变 pin 类型」再加虚覆盖；今天没有。

**连带（同 Feature 内小切片，勿另起全知表）**

- `Material.cpp` / `MaterialTestGraph` 里对 TextureObject / ScalarParameter 的探测：改为  
  - 读 pin 类型 / 或  
  - `Cast<MaterialGraphNodeDef_TextureObject>` **单点**业务查询（合法 B：知道要找哪一种类），**禁止**再堆中央 switch。

**Feature / 验收**

- 登记 `MAT-F07`（或挂在 CORE-F37-Sxx，但推荐独立 MAT）；薄 L2 Design 一页即可。
- 验收：`material-ir`；Editor 连线拒绝错误类型；梯子文件无 `dynamic_cast`。

**刻意不做**

- 不为 pin 类型引入 visitor / 反射查询表。
- 不把 MIR 类型系统与 pin DeclaredType 强行双写进磁盘（DeclaredType 运行时填充即可；反序列化后 ctor/工厂重建图时写好）。

---

### 7.2 簇 A2 — MIR Kind — **Design 已出，待审批**

**锁定方向（对齐 UE）：** `MIRValue::As<T>()` = `IsA(T::TypeKind) ? static_cast<T*> : nullptr`（及自由函数 `As<T>(MIRValue*)`）。  
**不是**手写一长串 `AsConstant`/`AsOperator` 自由函数为主 API（UE 用模板；你们已有 `TypeKind`）。

- **Design Spec:** [CORE-F37_MATERIAL_IR_KIND_CAST_DESIGN.md](../../Render/Material/CORE-F37_MATERIAL_IR_KIND_CAST_DESIGN.md)（`CORE-F37-S01`，Status **Review**）
- **UE 锚点:** `Engine/.../Materials/MaterialIR.h` — `FValue::As<T>()`；`MaterialIRDebug.cpp` 使用 `Value->As<FConstant>()` 等；MaterialIR 树内无 `dynamic_cast`
- **维护者裁定:** _待审批 → Planned 后方可实现_

---

### 7.3 簇 B1 — MEObject LazyRTTI（推荐默认 · 本战役只做 Runtime）

**本战役：B1-a（Runtime）机械换 `Cast<>`。**  
Editor / Tests 里同类 LazyRTTI **本战役放过**（与 B2 一并后置），避免和「Editor 边界暂缓」纠缠。

规则：

- 仅当静态基类是 `MEObject`（或 `shared_ptr` thereof）才换 `Cast<>`。
- 禁止在机械波里「顺手」改 A1/A2/C1。
- 不改 Load API 形状（仍 ByPath 返回基类 + Cast）。

---

### 7.4 簇 B2 — Editor 子模块 — **Deferred（维护者：本战役放过）**

无法在不改 Editor 模块模型的前提下「消灭」向下转；收益低。  
`FindSubModuleAs<T>` / typed 注册表 → **后置**，不进本战役执行序。

---

### 7.5 簇 C1 — RHI（推荐默认）

**接受暂留；登记一条 Open TD，不排期实现。**

- 文案大意：RHI 抽象→后端具体类型依赖 C++ RTTI；退出条件 = typed backend handle 或后端私有 downcast API 设计落地。
- **不**把 RHI 资源塞进 `MEObject`。
- 本战役零代码触碰 OpenGL/Vulkan cast。

---

### 7.6 本战役执行序（批准后 · Editor 已剔除）

```text
1) CORE-F37-S01  MIR As<T>()（本 Design，审批后）
2) B1-a          Runtime MEObject → Cast<>（机械）
3) A1            MAT-F07 pin 数据声明（另 Design）
4) C1            仅登记 TD
— 后置：B2 Editor；B1-b Editor/Tests
```

---

## Changelog

| Date | Note |
|------|------|
| 2026-10-15 | 初稿：218 处盘点 + A/B/C 簇；开放设计题；未改运行时代码 |
| 2026-10-15 | §7 Agent 推荐方案（A1 数据 pin / A2 Kind accessor / B1 分刀 / B2 模板 / C1 TD） |
| 2026-10-15 | 澄清 A2：`As*` 是药不是病；B2 + Editor 侧 B1-b 本战役 Deferred |
| 2026-10-15 | A2 锁定 UE `As<T>()`；Design Spec Review 待审批 |
