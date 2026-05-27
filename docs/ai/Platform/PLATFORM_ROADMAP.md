# Platform 路线图（UE 化大方向）

Last updated: 2026-05-26  
Status: **拍板** — 平台线 **P0/M1 底座、资产管线 P1–P6.1、P3 Undo 首轮、P2 主体（E0/E3/E4/E2/P6）已落地**；当前主攻 **E1 Inspector 统一** + **P7 产品集成**；详见 **§8 完成情况总览**

## 1) 产品方向

minEngine 目标从「渲染学习 demo」升级为 **更像 Unreal 的编辑器驱动引擎**：

- **Runtime**：可配置启动、清晰所有权与内存管理、反射驱动序列化（已有基础）
- **Editor**：Editor 平台化（Inspector / Previewer / Asset 基础设施）、Content Browser、Undo、模式化解耦
- **后续**：`MEFunction` + Lua；不阻塞当前平台线

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
P4  反射 MEFunction       → 设计待写
P5  Lua 脚本              → 设计待写
```

**原则：** 先让「路径 + 对象活着」可靠，再堆编辑器与脚本。

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
| 反射 | Property + MEReflection 序列化 | **P4** `MEFunction` 等价物 | （待写） |
| Undo | E1.1–E1.4 + BinaryArchive Property 路径 | E1.5 Material；TryMerge/Composite | [Command History](../Editor/EDITOR_COMMAND_HISTORY.md) |
| 序列化 | JSON 场景/资产 + **S1–S2** BinaryArchive / Property API | 更多磁盘格式与类型 | [Serialization](./Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md) |
| 脚本 | 无 | **P5** Lua 薄绑定 | （待写） |

## 4) 与 UE 对照（学习用）

| UE | minEngine 目标 |
|----|----------------|
| `FPaths` / `FConfigCache` | `PathRegistry` + Engine/Project config |
| `GUObjectArray` + GC | **`ObjectManager`** 弱引用索引 + `CollectGarbage(Domain)` |
| `UObject::Outer` | `MEObject::m_Outer` 系统化 |
| Asset Registry + Content Browser | `AssetManager` 变更 API + Browser UI |
| Details + Preview | Inspector Drawer + PreviewScene |
| `UFunction` + Blueprint | `MEFunction` + Lua（后期） |
| `FTransaction` / Undo | **`EditorCommandStack`**（E1.1–E1.4 已落地） |

## 5) 里程碑

| 里程碑 | 交付 | 状态 |
|--------|------|------|
| **M0** | `PathRegistry`、相对 Engine 配置、启动 resolve 日志 | **Done**（Playground 清理等见 §8 P0 延后） |
| **M1–M2** | `ObjectManager` weak 索引 + `CollectGarbage` + 削减 `RemoveObject` | **主干 Done**（C3 分域、目视验收见 §8 P1 延后） |
| **M3** | E0/E3/E4/P6/P6.1、P3 Undo 首轮、E2 Preview 核心、Appearance M0–M6b | **Done** |
| **M4** | E1 Inspector 统一、P7 Dock/Import/打开路由 | **进行中** |
| **M5+** | P4 `MEFunction`、P5 Lua | **未启动** |

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
| **P4 / P5** | MEFunction、Lua（设计未写） |

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

### P4 / P5 — 反射函数与脚本

| 状态 | 说明 |
|------|------|
| **未启动** | P4 `MEFunction`、P5 Lua：**设计案待写**；不阻塞当前 Editor 线 |

### 渲染 / 材质（维护轨，非 Platform 优先级）

| 已完成 | 延后 |
|--------|------|
| Phase **0–5**（IR/编译器、Material Editor E0–E4、PBR+IBL+Skybox） | Parallax、WPO、Translucent IBL、编辑器 Material **Undo**（归 P3 E1.5） |
| `--material-ir-test`、golden `MaterialIRSmoke.memtl` | 更多 `TextureSample` 变体、运行时材质批量工具 |

### 总结

平台线已从「渲染 demo」进入 **编辑器驱动**：**路径与对象底座（P0/P1）和资产闭环（P1–P6.1）已基本可用**，Editor 具备 Shell、Content Browser、主题排版、Scene/Material 双模式、**Undo 首轮** 与 **Inspector 3D 预览核心**。当前瓶颈在 **产品化收口**——**E1** 统一 Inspector 门面、**P7** Dock/Import/打开路由，以及预览与 Browser 的 **Texture/缩略图** 子项；**P4/P5** 仍为零。建议接下来保持 **E1 → P7 → E2 延后子项 → E1.5/Command E2** 顺序，避免并行铺开 CB 右键与 Material Undo。

---

## 9) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-23 | 初稿：UE 化方向、P0–P5 优先级、能力矩阵 |
| 2026-05-26 | 同步 P3 Undo、P6.1、E2 核心 Done；§6 延后表 |
| 2026-05-26 | **§8 完成情况总览**（对照各设计案 + 代码）；更新 §2/§3/§5；修正 P0/P1 已为 Done 主干 |
