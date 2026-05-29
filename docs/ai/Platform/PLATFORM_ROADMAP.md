# Platform 路线图（UE 化大方向）

Last updated: 2026-05-28  
Status: **拍板** — **P0/P1 与资产 Runtime（P1–P6.1）主干已完成**；**近期优先 §12 基建（CLI·Test·Verify）**；**P2 Editor / §11 Core 新切片延后至基建 M4**；详见 **§8**

## 1) 产品方向

minEngine 目标从「渲染学习 demo」升级为 **更像 Unreal 的编辑器驱动引擎**：

- **Runtime**：可配置启动、清晰所有权与内存管理、反射驱动序列化（已有基础）
- **Editor**：Editor 平台化（Inspector / Previewer / Asset 基础设施）、Content Browser、Undo、模式化解耦
- **Core 脚本线（规划中）**：先完成 **P4 函数反射**，再在此基础上讨论委托与 Lua（见 [Functions](./Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md)、[Delegates](./Reflection/REFLECTION_DELEGATES_DESIGN.md)、[Scripting](./Scripting/LUA_SCRIPTING_DESIGN.md)）

渲染/材质（[Render/Material](../Render/Material/MATERIAL_SYSTEM_ROADMAP.md)）维持维护，新功能以平台能力为主。

## 2) 优先级（与用户拍板一致）

```text
P0  启动 / 路径配置化     → ENGINE_STARTUP_DESIGN        [M0 Done]
P1  内存管理（非重型 GC） → MEMORY_MANAGEMENT_DESIGN     [M1/M2 主干 Done]
P2  Editor 平台化           → EDITOR_PLATFORM_PLAN（E0–E4）
    └─ E0 / E3 / E4 / P6 / P6.1-polish / Appearance M0–M6b [Done]
    └─ E2 Preview 核心（E2.1–E2.3a）[Done]
    └─ E1 Inspector 统一化 [进行中]
    └─ P7 默认 Dock / Import 路径 [待做]
P3  编辑器 Command/Undo   → EDITOR_COMMAND_HISTORY       [首轮 Done]
    └─ S1–S2 BinaryArchive + Property API [Done]
    └─ E1.1–E1.4 栈 + Scene + Inspector property + Snapshot [Done]
    └─ 延后：E1.5 Material Undo；Command E2（TryMerge/Composite）
P4  函数反射               → REFLECTION_FUNCTIONS_DESIGN         [设计中，可按阶段实现]
P5  委托（Delegate）       → REFLECTION_DELEGATES_DESIGN        [占位，依赖 P4 设计]
P6  Lua 脚本              → LUA_SCRIPTING_DESIGN               [占位，依赖 P4 设计]
```

**原则：** P0/P1 与资产 Runtime 已收口；**函数反射是脚本与事件的前置**；Editor E1/P7 可与 P4 并行但不应抢占 P4 首 PR。

## 3) 能力矩阵

| 能力 | 现状（2026-05-26） | 目标 / 缺口 | 设计文档 |
|------|---------------------|-------------|----------|
| 启动配置 | `PathRegistry`、相对 `EngineConfig`、CLI/ENV；`ProjectManager` 设 Content 根 | Playground 残余硬编码清理；Saved 路径类型化 | [Startup](./Startup/ENGINE_STARTUP_DESIGN.md) |
| 对象生命周期 | `ObjectManager` **weak_ptr** 索引、`CollectGarbage`、Scene 卸载 GC | Preview/Active **分域**根标记（C3）；减少业务 `RemoveObject` 遗留 | [MemoryManagement](./MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md) |
| 资产 | P1–P5 Registry CRUD、efsw、NFD、相对 `AssetPath` | 依赖图、异步 Reimport；Meta Property Inspector（E1） | [Asset Pipeline](./ContentBrowser/ASSET_PIPELINE_DESIGN.md) |
| Content Browser | P6 框架 + P6.1 树/网格/面包屑 + polish | 搜索/过滤、右键菜单、缩略图、Import 到当前路径 | [CB UI](./ContentBrowser/CONTENT_BROWSER_UI_DESIGN.md) |
| Editor 预览 | `PreviewScene`；Inspector Material/StaticMesh Scene3D | Texture2D 检视；CB Tile 缩略图 | [PREVIEWER_DESIGN](../Editor/PREVIEWER_DESIGN.md) |
| 编辑器 Shell | E0 模块/子模块/服务模块 | **P7** 默认 Dock、菜单与 Focus 链 | [EDITOR_SHELL_DESIGN](../Editor/EDITOR_SHELL_DESIGN.md) |
| Editor 外观 | M0–M6b 主题/字体/Property；排版默认 **3/4 字号** | M6c 材质图节点色；i18n；Appearance 设置 UI | [EDITOR_APPEARANCE](../Editor/EDITOR_APPEARANCE.md) |
| 反射（数据） | `MEClass`/`MEProperty`、代码生成、序列化/Inspector | 维护 | `Runtime/Core/Reflection/` |
| 反射（函数） | 无 | **P4** 函数反射（`MEFunction` 等） | [Functions Design](./Reflection/REFLECTION_FUNCTIONS_DESIGN.md) |
| Undo | E1.1–E1.4 + BinaryArchive Property 路径 | E1.5 Material；TryMerge/Composite | [Command History](../Editor/EDITOR_COMMAND_HISTORY.md) |
| 序列化 | JSON 场景/资产 + **S1–S2** BinaryArchive / Property API | 更多磁盘格式与类型 | [Serialization](./Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md) |
| 委托 | 无 | **P5** Delegate 系统 | [Delegates](./Reflection/REFLECTION_DELEGATES_DESIGN.md) |
| 脚本 | 无 | **P6** Lua | [Lua](./Scripting/LUA_SCRIPTING_DESIGN.md) |

## 4) 与 UE 对照（学习用）

| UE | minEngine 目标 |
|----|----------------|
| `FPaths` / `FConfigCache` | `PathRegistry` + Engine/Project config |
| `GUObjectArray` + GC | **`ObjectManager`** 弱引用索引 + `CollectGarbage(Domain)` |
| `UObject::Outer` | `MEObject::m_Outer` 系统化 |
| Asset Registry + Content Browser | `AssetManager` 变更 API + Browser UI |
| Details + Preview | Inspector Drawer + PreviewScene |
| `UFunction` + `ProcessEvent` | （规划中）`MEFunction` + `MEObject::ProcessEvent`（P4） |
| `DECLARE_DELEGATE` / 动态多播 | （规划中）`MEDelegate` 单播 / 多播（P5） |
| UnLua / LuaMachine | （规划中）`ScriptSubsystem` + Lua（P6） |
| `FTransaction` / Undo | **`EditorCommandStack`**（E1.1–E1.4 已落地） |

## 5) 里程碑

| 里程碑 | 交付 | 状态 |
|--------|------|------|
| **M0** | `PathRegistry`、相对 Engine 配置、启动 resolve 日志 | **Done**（Playground 清理等见 §8 P0 延后） |
| **M1–M2** | `ObjectManager` weak 索引 + `CollectGarbage` + 削减 `RemoveObject` | **主干 Done**（C3 分域、目视验收见 §8 P1 延后） |
| **M3** | E0/E3/E4/P6/P6.1、P3 Undo 首轮、E2 Preview 核心、Appearance M0–M6b | **Done** |
| **M4** | E1 Inspector 统一、P7 Dock/Import/打开路由 | **并行维护**（非 Core 主线） |
| **M5** | P4：函数反射方案设计 + 最小实现 | **未启动 — Core 主线（设计期）** |

## 6) 延后功能索引（简表）

> 各模块 **已完成 vs 延后** 的完整说明见 **§8**。下表仅作快速跳转。

| 项 | 说明 |
|----|------|
| **E1** | `InspectorTarget`、Drawer、Asset Meta → PropertyWidgets |
| **E2.2b / E2.3b / E2.4** | Texture Inspector 预览；CB 缩略图；Material 视口相机 |
| **E1.5 / Command E2** | Material Undo；TryMerge / Composite |
| **P7** | 默认 Dock、Import 到当前 Browser 路径、双击打开 Editor 路由 |
| **CB 后置** | 搜索/过滤、右键菜单（含 Delete）、拖拽 Import |
| **P0 尾项** | Playground 路径清理、Saved 目录类型化 |
| **P1 尾项** | GC 分域（C3）、Editor 场景反复开关目视验收 |
| **Appearance 后置** | M6c、i18n、Appearance 设置 UI |
| **P4** | 函数反射设计与切片：见 [REFLECTION_FUNCTIONS_DESIGN](./Reflection/REFLECTION_FUNCTIONS_DESIGN.md) |
| **P5** | 委托：见 [REFLECTION_DELEGATES_DESIGN](./Reflection/REFLECTION_DELEGATES_DESIGN.md) |
| **P6** | Lua：见 [LUA_SCRIPTING_DESIGN](./Scripting/LUA_SCRIPTING_DESIGN.md) |

---

## 7) 参考

- [docs/ai/README.md](../README.md) — 文档布局
- [Editor 平台化规划](../Editor/EDITOR_PLATFORM_PLAN.md)
- [Render/Material 路线图](../Render/Material/MATERIAL_SYSTEM_ROADMAP.md)

---

## 8) 完成情况总览（2026-05-26）

> 对照：`EDITOR_PLATFORM_PLAN.md`、`ASSET_PIPELINE_*`、`ENGINE_STARTUP_DESIGN.md`、`MEMORY_MANAGEMENT_DESIGN.md`、`EDITOR_COMMAND_HISTORY.md`、`CONTENT_BROWSER_UI_DESIGN.md`、`MATERIAL_SYSTEM_ROADMAP.md` 与当前 `master` 代码。

### P0 — 启动 / 路径配置

| 已完成（主要） | 延后 |
|----------------|------|
| `PathRegistry`（Engine/Project 根、相对 resolve、CLI `--engine-root` / `--engine-config`、ENV） | Playground 等处 **残余硬编码** 清理 |
| `EngineConfig.meconfig` **相对** `EngineDefaultAssetsRoot` | Saved / 缓存路径在 `PathRegistry` 上 **类型化**（设计 §M0 尾项） |
| `ProjectManager::OpenProject` 设置 `ProjectContent`；资产扫描用绝对 resolve | 启动失败时的 **UI 级** 提示（仍多为日志） |

### P1 — 内存管理（轻量 GC）

| 已完成（主要） | 延后 |
|----------------|------|
| `ObjectManager` 注册表改为 **`weak_ptr`**；`FindObject` lock 语义 | **Preview / Active 分域** 根标记（设计 C3） |
| `CollectGarbage` / `CollectGarbageWithEngineRoots`；Scene 卸载走 GC | Editor **多次开关场景** 泄漏目视验收（设计 §7 未勾） |
| 业务侧削减显式 `RemoveObject`（Scene/GO/Preview 等） | `MaterialIRTest` golden 与 GC 交互（设计待修项） |
| `--object-manager-test` 头less 验收 | 与 **P4 脚本** 共拟的 `ObjectHandle` 等 M3 句柄 API |

### P2 — Editor 平台化

#### E0 Editor Shell

| 已完成 | 延后 |
|--------|------|
| `Editor` bootstrap、SubModule（Scene/Material）互斥、`EditorServiceModule` | **P7** 默认 Dock 布局固化、Window 菜单全面接线 |
| `GUIManager` 工厂；模块自管 layout | Material 模式下 **可选打开 CB**（P7 可选） |

#### E3 资产基础设施 + E4 FileDialog

| 已完成 | 延后 |
|--------|------|
| **P1–P2** `AssetTypeRegistry`、`AssetManager` CRUD/事件、相对 `AssetPath`、Import | **依赖图**、异步 Reimport 队列 |
| **P3** `IFileDialogService` + NFD（Windows） | macOS / Linux 对话框 |
| **P4** `AssetWorkflowModule` Import 编排 | Import 目标目录跟 **当前面包屑**（P7） |
| **P5** `ProjectAssetWatcher`（efsw + debounce） | P5.1 `Reimported` 事件细化（可选） |
| Move/Rename API + `--asset-manager-test` | Move/Rename **UI**、删除引用完整性阻塞 |

#### P6 / P6.1 Content Browser

| 已完成 | 延后 |
|--------|------|
| `ContentBrowserModule`、`AssetTreeModel`（树含目录+资产）、Registry 订阅 | **搜索 / 类型过滤** |
| Tile 网格、面包屑、Import/Refresh；**§2.6 polish**（扁平面包屑 + 方 Icon + 单行省略） | **右键菜单**（Delete/Reveal/Rename/Move） |
| Scene 默认 Dock CB 位（Hierarchy/Inspector 下、Console 右）；Material 模式隐藏 CB | **双击打开** SubModule（仅 Log hook） |
| Inspector 聚焦路由；单击目录 **不清空** 资产选中 | **E2.3b** Tile 缩略图 |

#### Editor Appearance（并行轨）

| 已完成 | 延后 |
|--------|------|
| M0–M6b：主题 Dark/Light、`EditorAppearance`、`PropertyWidgets`、`TransformWidget` | **M6c** 材质图节点域主题色 |
| M5–M5.1：Font 资产、多角色排版、`EditorTypographyScope`、CJK glyph 合并 | **i18n**、Appearance **设置 UI** |
| M6：`EditorThemeScope`、Hierarchy 语义选中色 | **M7** Object 引用 Picker（非 Asset） |
| 默认排版字号 **×0.75**（2026-05-26） | — |

#### E2 Previewer

| 已完成 | 延后 |
|--------|------|
| `PreviewScene` + `SceneViewport` 桥接（E2.1） | **E2.2b** Texture2D Inspector 预览 |
| `InspectorAssetInspection`：Material / StaticMesh **Scene3D** 方槽（E2.2 / E2.3a） | **E2.3b** CB Tile 缩略图 |
| Material 编辑视口仍独立 `MaterialEditorViewportClient` | **E2.4** Material 视口飞行/轨道 |

#### E1 Inspector（当前主攻）

| 已完成 | 延后 |
|--------|------|
| `InspectorWindow` 多 `IEditorInspectorSource` 路由（Scene / Material / CB 资产） | **`InspectorTarget`** 统一模型 |
| Scene/Material：**PropertyWidgets** + `PropertyEditPolicy` + 属性 Undo（E1.3） | **Drawer 注册表**（替代各 Source 各画一套） |
| CB 聚焦 → `AssetWorkflowInspectorSource`（Meta 字段） | Asset Meta → **PropertyWidgets**（现为裸 `ImGui::Text`） |
| `InspectorAssetInspection` 预览与 E2 共用 `PreviewScene` | **多选**、Unsupported 类型 UX |

#### P7 产品集成（未做）

| 计划交付 | 说明 |
|----------|------|
| 默认 Dock / 布局保存策略 | 与 `MyMEProjectSettings` 对齐 |
| Import 到 **当前 Browser 目录** | 替代 v0 固定 `Imported/` |
| `OnContentBrowserAssetActivated` → 按类型打开 Scene/Material 等 | 取代仅 Log |

### P3 — Editor Undo / 序列化

| 已完成 | 延后 |
|--------|------|
| **S1–S2** `BinaryArchive`、公开 Property 序列化 API | 更多磁盘格式 / 类型覆盖 |
| **E1.1** `EditorCommandStack` + 项目 `MaxUndoStackDepth` | **Command E2**：`TryMerge`、Composite、菜单描述 |
| **E1.2** Scene 结构 Command（GO/Component/Rename/Transform） | 大快照 **字节预算**（设计评估项） |
| **E1.3–E1.4** Inspector 属性 Undo、GO/Component Snapshot | **E1.5** Material 图/节点/连线 Undo |

### P4 — 反射函数与委托

| 已完成 | 下一里程碑 |
|--------|------------|
| **Property 线**：`MEClass`/`MEProperty`、header tool、`FinalizeReflection`、序列化/Inspector/Undo 消费 | **P4.1** `MEFunction` 类型 + `MEClass::AddFunction` |
| [Enum Property](./Reflection/REFLECTION_ENUM_PROPERTY_PLAN.md) Size/绑定 | **P4.2** `ProcessEvent` + 参数缓冲 + `--reflection-function-test` |
| | **P4.3** `ME_FUNCTION` + tool 生成 |
| | **P4.4–P4.5** `MEDelegate` |

设计：[REFLECTION_FUNCTIONS_DESIGN.md](./Reflection/REFLECTION_FUNCTIONS_DESIGN.md)

### P5 — Lua 脚本

| 状态 | 说明 |
|------|------|
| **未启动** | 依赖 **P4.1–P4.3**；默认 **sol2**；`ScriptSubsystem` 生命周期 |

设计：[LUA_SCRIPTING_DESIGN.md](./Scripting/LUA_SCRIPTING_DESIGN.md)

### 渲染 / 材质（维护轨，非 Platform 优先级）

| 已完成 | 延后 |
|--------|------|
| Phase **0–5**（IR/编译器、Material Editor E0–E4、PBR+IBL+Skybox） | Parallax、WPO、Translucent IBL、编辑器 Material **Undo**（归 P3 E1.5） |
| `--material-ir-test`、golden `MaterialIRSmoke.memtl` | 更多 `TextureSample` 变体、运行时材质批量工具 |

### 总结

平台线已从「渲染 demo」进入 **编辑器驱动**：**P0/P1 与资产 Runtime 已基本收口**。Editor 侧 **E1/P7** 等产品化可 **并行维护**。**Core 下一主线** 为 **P4（函数反射 + 委托）→ P5（Lua）**，顺序 **不可颠倒**（Lua 绑定 `MEFunction`，委托事件与 `ProcessEvent` 共用参数路径）。建议实施：**P4.1 → P4.2 → P4.3 → P5.0–P5.2 → P5.3 → P4.4 委托 → P5.4 ScriptComponent**。

---

## 9) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-23 | 初稿：UE 化方向、P0–P5 优先级、能力矩阵 |
| 2026-05-26 | 同步 P3 Undo、P6.1、E2 核心 Done；§6 延后表 |
| 2026-05-26 | **§8 完成情况总览**（对照各设计案 + 代码）；更新 §2/§3/§5；修正 P0/P1 已为 Done 主干 |
| 2026-05-27 | **§11 Core 主线**：P4/P5 设计案落盘；§2/§3/§5/§8 对齐函数反射+委托+Lua |

## 10) 2026-05-27 Editor 任务安排（滚动，可并行）

> Editor 产品化；**不替代** §11 Core 主线。

> 目的：按 ROADMAP 顺序推进产品化收口；今天任务为滚动队列，可继续追加，不在此处锁死全部范围。

| 顺位 | 模块 | 今日推进点（简要） | 备注 |
|------|------|-------------------|------|
| 1 | **P2/P7 + E1** | 右键菜单 M1 CB 收口（Delete/Import/Refresh）；Reveal/Rename 暂缓+待删代码；再 M2/M3 | 设计 §15.4 |
| 2 | **P2 / CB UI** | CB Tile 引入 Icon（替代占位方块） | 先静态类型 Icon，缩略图仍归 E2.3b |
| 3 | **P3 E1.5 + E2.4** | Material Editor 编辑体验补强：Undo 主链路 + Preview 视口飞行 | 建议先 Undo，再飞行相机 |

关联推进案：[`docs/ai/Editor/EDITOR_TASK_ROLLOUT_2026-05-27.md`](../Editor/EDITOR_TASK_ROLLOUT_2026-05-27.md)

---

## 11) 2026-05-27 Core 主线 — 函数反射 · 委托 · Lua

> **用户拍板：** P0/P1/资产 Runtime 已做完；`master` 上推进 **引擎 Core 脚本栈**。

| 顺位 | 模块 | 阶段 | 设计文档 |
|------|------|------|----------|
| 1 | **P4.1–P4.2** | `MEFunction` + `ProcessEvent` + 测试 | [REFLECTION_FUNCTIONS_DESIGN](./Reflection/REFLECTION_FUNCTIONS_DESIGN.md) |
| 2 | **P4.3** | `ME_FUNCTION` + header tool | 同上 |
| 3 | **P5.0–P5.2** | Lua VM + `MEObject`/Property 绑定 | [LUA_SCRIPTING_DESIGN](./Scripting/LUA_SCRIPTING_DESIGN.md) |
| 4 | **P5.3** | Lua 调 `MEFunction` | 同上 |
| 5 | **P4.4** | 单播 `MEDelegate` | 函数设计 §4–§6 |
| 6 | **P5.4** | `ScriptComponent` / `.melua` | Lua 设计 §1.1 |
| 7 | **P4.5 / P5.5** | 多播委托、Lua 事件（可选） | — |

**硬依赖：** P5.\* 依赖 P4.1–P4.3；委托 Broadcast 依赖 `ProcessEvent` 参数封送。

**与 Editor 并行：** E1/P7/CB 不阻塞 P4；Inspector 对 Callable 的调试按钮可放在 P4.3 之后小切片。

---

## 12) 2026-05-28 基建主线（CLI · Test · Verify）— 当前优先

> **拍板：** 近期不堆 Editor/产品功能；先完成 [INFRASTRUCTURE_ROADMAP](./INFRASTRUCTURE_ROADMAP.md) 至 **M4**（`CLI-F01` → `TEST-F01` → `scripts/verify` + DoD）。
>
> - 已完成：`BOOTSTRAP_DIGEST`、`TECH_DEBT`（不进该路线图正文）。
> - **§11 新切片暂停：** 反射已有 MVP 可维护；P4.3+ / P5 / Lua 等 **M4 之后**再开（见 [TECH_DEBT](../TECH_DEBT.md) TD-005、TD-006）。
> - **§10 Editor 滚动任务：** 维护级修 bug 可做；新 E1/P7 大项排队。

| 顺位 | Feature | 里程碑 |
|------|---------|--------|
| 1 | `CLI-F01` | M1–M2：解析/注册 + 首个 test 迁移 |
| 2 | `TEST-F01` | M3–M4：全量迁移 + smoke/full + verify |

变更记录：2026-05-28 新增 §12。
