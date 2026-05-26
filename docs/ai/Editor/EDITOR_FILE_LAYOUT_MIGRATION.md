# Editor / Runtime 文件分层迁移计划

Last updated: 2026-05-26  
Status: **已实施（2026-05-26）— Editor + Runtime Loader 迁移**  
关联：[EDITOR_SHELL_DESIGN.md](./EDITOR_SHELL_DESIGN.md)、[PREVIEWER_DESIGN.md](./PREVIEWER_DESIGN.md)、[RESOURCE_PIPELINE_PLAN.md](../Render/RESOURCE_PIPELINE_PLAN.md)

---

## 0) 审批摘要（拍板项）

| # | 范围 | 决策（用户 2026-05-26） |
|---|------|-------------------------|
| 1 | Inspector | 仅把 `Services/` **根下** Inspector 相关文件收进 **`Services/Inspector/`**；**不移动** `UI/Inspector/` |
| 2 | SubEditor | 仅 **Editor 侧**、**非 UI** 的 Material / Scene 代码迁入 `SubEditor/`；**ViewportClient** 跟各自子编辑器；**不触碰** Runtime `MaterialEdGraph` 等 |
| 3 | TextureCubeLoader | **本批不动**（仍留在 `Render/`） |
| 4 | MaterialLoader | 合并为 **单一对外加载函数**；删除 `MaterialAssetLoader` |
| 5 | 实施 gate | **本文档审批通过后**再改代码 |

---

## 1) Inspector：`Services/` 根 → `Services/Inspector/`

### 1.1 现状

```text
Editor/src/Services/
  InspectorModule.{h,cpp}          ← 在 Services 根
  Inspector/
    InspectorAssetInspection.{h,cpp}
Editor/src/UI/Inspector/           ← 本批不动
  InspectorPreviewPresenter.{h,cpp}
```

### 1.2 目标

```text
Editor/src/Services/Inspector/
  InspectorModule.{h,cpp}          ← 从 Services 根移入
  InspectorAssetInspection.{h,cpp}   ← 已在位
Editor/src/UI/Inspector/           ← 不变
```

### 1.3 操作

- `git mv` `Services/InspectorModule.*` → `Services/Inspector/InspectorModule.*`
- 批量替换 include：
  - `"Services/InspectorModule.h"` → `"Services/Inspector/InspectorModule.h"`
- **不变**：类名 `InspectorModule`、`kModuleId`、`EditorServiceModule` 基类、`InspectorAssetInspection` API
- **不移动**：`UI/Inspector/InspectorPreviewPresenter.*`（仍 `#include "Services/Inspector/InspectorAssetInspection.h"` 等）

### 1.4 主要受影响文件（预估）

- `Editor.h` / `Editor.cpp`
- `Services/AssetWorkflowModule.cpp`
- `Shell/IEditorContext.h`
- `Services/Inspector/InspectorAssetInspection.cpp`（若自 include 路径需核对）

### 1.5 风险

**低** — 纯路径搬迁，无行为变更。

---

## 2) SubEditor：Editor 侧 Material / Scene（非 UI）

### 2.1 范围说明（避免误解）

| 做 | 不做 |
|----|------|
| `Editor/src/Material/*` → `Editor/src/SubEditor/Material/*` | **不**把 Runtime `Function/Render/Material/MaterialEdGraph*` 等迁入 Editor |
| `Editor/src/Scene/*` → `Editor/src/SubEditor/Scene/*` | **不**移动 `UI/EditorWindows/` 下任何窗口（MaterialGraph、MaterialEditorViewport、SceneEditingViewport 等） |
| `Viewport/MaterialEditorViewportClient.*` → `SubEditor/Material/` | **不**移动 `Editor/src/Preview/`（Material / Inspector 共用） |
| `Viewport/SceneEditingViewportClient.*` → `SubEditor/Scene/` | **不**移动 `Commands/Scene/`（Undo 命令包，本批保持原位） |

Editor 内 `MaterialGraphIds`、`MaterialGraphNodeRegistry` 等是 **imgui-node-editor 与 Editor 注册表**，属于 Editor 子编辑器逻辑，迁到 `SubEditor/Material/` 合理；与 Runtime 材质图 IR **无关**。

### 2.2 迁入 `SubEditor/Material/` 的文件清单

```text
MaterialEditor.{h,cpp}
MaterialEditorSession.h
MaterialEditorInspectorSource.{h,cpp}
MaterialGraphIds.{h,cpp}
MaterialGraphNodeRegistry.{h,cpp}
MaterialCompileDiagnosticsDrawer.{h,cpp}
MaterialNodeDefPropertyDrawer.{h,cpp}
MaterialEditorViewportClient.{h,cpp}    ← 自 Viewport/
```

### 2.3 迁入 `SubEditor/Scene/` 的文件清单

```text
SceneEditor.{h,cpp}
SceneEditorInspectorSource.{h,cpp}
SceneEditingViewportClient.{h,cpp}      ← 自 Viewport/
```

### 2.4 保留原位

```text
Editor/src/Viewport/
  EditorViewportClient.{h,cpp}          # 共享基类
  EditorViewportTypes.h

Editor/src/UI/EditorWindows/            # 全部窗口实现
Editor/src/Preview/                     # PreviewScene 共享
Editor/src/Commands/Scene/              # 场景 Undo 命令
```

### 2.5 Include 前缀（机械替换）

| 旧 | 新 |
|----|-----|
| `Material/...` | `SubEditor/Material/...` |
| `Scene/...` | `SubEditor/Scene/...` |
| `Viewport/MaterialEditorViewportClient.h` | `SubEditor/Material/MaterialEditorViewportClient.h` |
| `Viewport/SceneEditingViewportClient.h` | `SubEditor/Scene/SceneEditingViewportClient.h` |

### 2.6 主要受影响文件（预估）

- `Shell/ViewportClientRegistry.{h,cpp}`
- `SubEditor/Scene/SceneEditor.cpp`（原 `Scene/SceneEditor.cpp`）
- `SubEditor/Material/MaterialEditor.cpp`
- `UI/EditorWindows/*`（仅 include 路径）
- `Commands/Scene/*.cpp`（`#include "SubEditor/Scene/SceneEditor.h"`）
- `HierarchyWindow.h` 等引用 `SceneEditor` 的 UI

### 2.7 风险

**中** — 面广但仍是 `git mv` + include；逻辑不改。注意 `ViewportClientRegistry` 与模式切换（Scene ↔ Material）include 链扫全。

---

## 3) Runtime Loader：归位 `Resource/Loaders/`（TextureCube 除外）

### 3.1 本批范围

| 动作 | 文件 |
|------|------|
| 迁入 `Runtime/Resource/Loaders/` | `ImageLoader`、`MeshLoader`、`MaterialLoader`、`SceneLoader`、`FontLoader`（自 `Resource/` 根） |
| 自 `Render/` 迁入 | `StaticMeshLoader`、`Texture2DLoader` |
| 合并删除 | `MaterialAssetLoader` → 并入 `MaterialLoader`（见 §4） |
| 新建 | `ShaderLoader`（自 `AssetManager.cpp` 抽出 `LoadAsset_Impl<Shader>`） |
| **不动** | **`TextureCubeLoader`**（仍在 `Render/`） |

### 3.2 目标目录

```text
Runtime/Resource/Loaders/
  ImageLoader.*
  MeshLoader.*
  MaterialLoader.*
  SceneLoader.*
  FontLoader.*
  ShaderLoader.*          # 新建
  StaticMeshLoader.*      # 从 Render/ 迁入
  Texture2DLoader.*       # 从 Render/ 迁入

Runtime/Function/Render/
  TextureCubeLoader.*     # 本批不碰
  Environment/*           # 仍引用 TextureCubeLoader / Texture2DLoader（新路径）
```

### 3.3 `AssetManager` 瘦身（本批目标）

- `LoadAsset_Impl<T>` 特化 **仅** 留在各 `*Loader.cpp`
- `AssetManager.cpp` 去掉 `LoadAsset_Impl<Shader>` 实现及对 `RenderSystem` 的直接依赖（Shader 迁出后）
- include 统一为 `Runtime/Resource/Loaders/XXXLoader.h`

### 3.4 各 Loader 行为（不变）

| Loader | 行为保持 |
|--------|----------|
| `StaticMeshLoader` | `MeshLoader::ImportFromFile` → GPU VB/IB → `StaticMesh` |
| `Texture2DLoader` | `ImageLoader::Load` → RHI → `Texture2D`；`CreateFromPixels` / `CreateFromHdrPixels` API 保留 |
| `SceneLoader` / `FontLoader` | 逻辑不变，仅路径 |
| `TextureCubeLoader` | **零变更** |

### 3.5 实施分期（建议单次 PR 内按 commit 切分）

1. **R-L0**：创建 `Resource/Loaders/`，迁移已在 Resource 根的 Loader  
2. **R-L1**：`MaterialLoader` 合并（§4）+ 删 `MaterialAssetLoader`  
3. **R-L2**：迁 `StaticMeshLoader`、`Texture2DLoader`，更新 `Texture.h` friend、Environment include  
4. **R-L3**：`ShaderLoader` 抽出，瘦身 `AssetManager.cpp`  

### 3.6 验收

- `cmake --build minEngine/build --target Editor`
- `Editor.exe --asset-manager-test`
- `Editor.exe --material-ir-test`
- 目视：场景视口、材质编辑、Inspector 3D 预览、贴图/网格加载、IBL/天空盒（TextureCube 路径未改）

---

## 4) MaterialLoader：合并为单一函数

### 4.1 现状

```text
MaterialLoader::LoadDeserialized(meta)     # Resource：JSON + FinalizeGraphAfterLoad，无 GPU compile
MaterialAssetLoader::LoadFromAssetMeta()   # Render：LoadDeserialized + MaterialCompiler::Compile
AssetManager::LoadAsset_Impl<Material>     # 委托 MaterialAssetLoader
```

全仓库 **`LoadDeserialized` 仅被 `MaterialAssetLoader` 调用**；无「只反序列化、不编译」的其它生产路径。

### 4.2 目标 API（推荐）

与 `SceneLoader::Load` 对齐，**只保留一个对外入口**：

```cpp
// Runtime/Resource/Loaders/MaterialLoader.h
class MaterialLoader
{
public:
    /** Deserialize .memtl, finalize graph, compile GPU material. Returns nullptr on any failure. */
    static std::shared_ptr<Material> Load(const AssetMeta& meta, std::string* outError = nullptr);
};
```

实现顺序（与现 `MaterialAssetLoader` **完全一致**）：

1. `NewObject<Material>` + `Serializer::FromFile` + `FinalizeGraphAfterLoad`（原 `LoadDeserialized` 体）
2. `RenderSystem::Get().GetRHI()` — 不可用则 error 并 `nullptr`
3. `MaterialCompiler::Compile(*material, ctx)` — 失败打 `m_LastCompileDiagnostics` 日志
4. 返回 `material`

`AssetManager::LoadAsset_Impl<Material>` 改为：

```cpp
return MaterialLoader::Load(meta);
```

### 4.3 删除项

- `MaterialAssetLoader.{h,cpp}` 整文件删除
- `MaterialLoader::LoadDeserialized` **不保留** 为 public API（逻辑内联进 `Load` 的 private 实现段即可，无需单独类名）

若将来需要「仅磁盘、不编译」（例如工具链），再增 `LoadDeserializedOnly` **private** 或独立 `MaterialDiskLoader`；**本批 YAGNA**。

### 4.4 与 Editor 双次 Compile 的说明（不本批修改）

`MaterialEditor::OpenSession` 当前 `LoadAsset<Material>` 后另调 `material->Compile()`。合并 Loader 后 `LoadAsset` 已含 compile，存在 **潜在重复 compile**——属既有行为；本迁移 **不改变** 该路径，避免夹带行为变更。若需优化，单开任务。

### 4.5 功能不变性

- 同一 `AssetMeta` 路径、同一错误日志语义、同一 `nullptr` 失败行为
- `LoadAsset<Material>` 对外行为不变

---

## 5) 建议实施顺序

```text
Phase A  Editor Inspector 路径整理（§1）
Phase B  Editor SubEditor + ViewportClient（§2）
Phase C  Runtime Loaders R-L0 → R-L3（§3–§4），TextureCube 跳过
```

Editor（A+B）与 Runtime（C）可：

- **同一 PR**（便于一次编译验收），或  
- **两 PR**：先 A+B 再 C（用户指定；默认待审批）

---

## 6) 实施检查表

- [x] §1 `InspectorModule` 位于 `Services/Inspector/`，`UI/Inspector` 未动  
- [x] §2 `SubEditor/Material`、`SubEditor/Scene` 文件齐全；`Viewport/` 仅剩基类；Runtime Material 树未动  
- [x] Editor：`cmake --build minEngine/build --target Editor` 通过（2026-05-26）  
- [x] §3 `TextureCubeLoader` 仍在 `Render/`（未动）  
- [x] §4 已删 `MaterialAssetLoader`；`MaterialLoader::Load` 单入口  
- [x] Runtime：`cmake --build minEngine/build --target minEngine Editor` 通过  
- [x] `Editor.exe --asset-manager-test`、`--material-ir-test` exit 0  
- [ ] 目视验收（场景/材质/Inspector 预览）  
- [ ] `PROGRESS_LOG.md` 追加一条

---

## 7) 修订记录

| 日期 | 说明 |
|------|------|
| 2026-05-26 | 初稿：按用户澄清 Inspector / SubEditor 范围、TextureCube 不动、MaterialLoader 单函数 |
