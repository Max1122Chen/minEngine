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
本文只整理证据与开放设计题；**A 类等你给出方案后再开 Design Spec / 实现**。禁止用 `Cast<>` 梯子替换 A 类全知函数。

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
| **维护者方案** | _（待填）_ |
| **建议后续** | 独立 Feature（候选 `MAT-F07`）+ 薄 L2 Design；验收：`material-ir` + 删梯子 |

### 簇 A2 — MIR 已有 Kind 仍用 `dynamic_cast` — **NeedsDesign（轻）或 Mechanical-if-policy**

| | |
|--|--|
| **证据** | `MIRDebug.cpp`、`MIREmitter.cpp`、`GLSLMaterialTranslatorImpl.cpp`：对 `MIRConstant` / `MIROperator` / … `dynamic_cast` |
| **为何偏 A** | `MIRValue` 已有 `MIRValueKind` + `IsA(Kind)` + 部分 `AsDimensional`；应用 Kind 守卫 + `static_cast`，不必 C++ RTTI |
| **开放问题** | ① 是否统一补齐 `AsConstant` / `AsOperator` / … 助手？② Debug 路径是否允许保留 dynamic_cast（性能无关）？ |
| **维护者方案** | _（待填）_ |
| **建议后续** | 政策拍板后小切片；勿与 A1 绑同一 PR |

### 簇 B1 — MEObject / Asset 图 LazyRTTI — **Mechanical（可批）**

| | |
|--|--|
| **证据** | Loaders（Material/Scene/Prefab/Sprite/Texture/Anim…）、`AssetManager`、`Prefab*`、`SceneManager`、`GameObject`/`Component` Outer、`Serializer`、部分 Tests |
| **模式** | `dynamic_cast<T*>(meObject)` / `std::dynamic_pointer_cast<T>(sp)`，T : MEObject |
| **修法** | `#include ObjectCast.h` → `Cast<T>(...)` |
| **风险** | 低；需确认无非 MEObject 误用 |
| **维护者确认** | _允许开机械波？Y/N；是否分 Runtime / Editor / Tests 多 PR？_ |

### 簇 B2 — Editor 子模块 id→typed — **NeedsDesign（轻）或 Mechanical**

| | |
|--|--|
| **证据** | `EditorContextHelpers.h`、`EditorDocumentTypes.cpp`：`FindSubModule(id)` 返回基类后再 `dynamic_cast<SceneEditor*>` 等 |
| **为何偏 B** | id 已选定具体模块；cast 多为确认。非闭世界全知表 |
| **可选改善** | `FindSubModuleAs<T>(id)` / 注册表存 typed 指针，减少重复 cast |
| **开放问题** | ① 保持现状 + 可接受？② 要模板 API？子模块是否进 MEObject 反射？ |
| **维护者方案** | _（待填）_ |

### 簇 C1 — RHI 后端 downcast — **Legitimate（暂留）**

| | |
|--|--|
| **证据** | `OpenGLRHI.cpp`、`VulkanRHI.cpp`、`VulkanRHIResources.cpp`：`RHI*` → `OpenGL*` / `Vulkan*` |
| **为何 C** | RHI 资源非 MEObject；当前抽象即“后端实现类” |
| **长期可选** | typed handle / CRTP / 后端私有接口；**非本战役必做** |
| **维护者确认** | _接受暂留？是否登记 TD？_ |

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
2. **先写 A1（Material pin）方案** — 影响最大、也是你举例的核心。  
3. 拍板 A2（MIR Kind）政策。  
4. 确认 B1 机械波是否立即开工（可与 A 并行，不同 PR）。  
5. B2 / C1 接受或 Deferred。

审阅完成后：把填好的「维护者方案」留下；需要 L2 Design 的簇再拆 `*_DESIGN.md` 并改 Registry Status。

---

## 6) 工作区备忘（实现前）

| 项 | 状态 |
|----|------|
| 反射 `Cast<>` 已存在 | 是 — `ObjectCast.h` |
| 注释 ID 卫生（WF-F05） | 已合入 master |
| 本 Survey | Review — 等方案 |
| 实现 | **Blocked on maintainer designs for A\*** |

---

## Changelog

| Date | Note |
|------|------|
| 2026-10-15 | 初稿：218 处盘点 + A/B/C 簇；开放设计题；未改运行时代码 |
