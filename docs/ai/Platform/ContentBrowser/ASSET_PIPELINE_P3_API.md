# Asset Pipeline — P3 接口定稿（审批用）

Last updated: 2026-05-25  
Status: **已批准**（分层：Runtime `Platform` + Editor 消费；submodule 已添加，实现待 P3a–P3d）  
前置：**P1/P1b** `8c5958d`、**P2** `7758c60`（`feat/editor-asset-workflow`，已合并 appearance M1–M3）  
父文档：[ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md) §6（E4）、§9 P3、§10 D8  
P2 定稿：[ASSET_PIPELINE_P2_API.md](./ASSET_PIPELINE_P2_API.md)

---

## 0) P2 完成度核对

| P2 项 | 状态 |
|-------|------|
| `DeleteAsset` / `MoveAsset` / `RenameAsset` | ✅ |
| `UnregisterAsset` / `ClearProjectRegistry` | ✅ |
| `Moved` 事件 | ✅ |
| Scene `UnregisterScene` on delete | ✅ |
| `--asset-manager-test` | ✅ |
| `IFileDialogService` / NFD | ❌ → **P3** |
| `AssetWorkflowModule::ImportAssetDialog` | ❌ → **P4** |

---

## 0.1) 分层决策（P3 拍板）

| 问题 | 决策 |
|------|------|
| FileDialog 放哪？ | **`Runtime/Platform/FileDialog/`**（引擎 OS 能力，非 Editor 专属） |
| 谁链 NFD？ | **`minEngine` Runtime 共享库**（`target_link_libraries(minEngine PRIVATE nfd)`），**不是**仅 Editor |
| 谁调对话框？ | **Editor / Playground / 未来游戏** 经 `FileDialogService::Get()` 或 `Engine` 访问 |
| 资产 filter 放哪？ | **`AssetTypeRegistry`**（`Runtime/Resource`）— 无资产语义的 filter **结构体** 定义在 Platform |
| Editor 职责 | 验收入口（MainMenu/Debug）、P4 编排；**不**实现 NFD |

**依赖方向（硬约束）：**

```text
Runtime/Platform/FileDialog  →  Core only
Runtime/Resource             →  Platform/FileDialog（仅 FileDialogTypes，见 §3）
Editor                       →  Runtime（Engine、Registry、Platform）
Platform                     →  ✗ Resource / ✗ Editor
```

**动机（简）：** 用户游戏日后也可能打开系统对话框（例如上传图片）；与 `AssetManager` 在 Runtime、Editor 编排的模式一致。

---

## 1) P3 交付边界

### 1.1 包含

| 项 | 说明 |
|----|------|
| **E4 类型** | `FileDialogTypes.h` — `FileDialogFilter` / `Request` / `Result`（无资产字段） |
| **E4 抽象** | `IFileDialogService` |
| **E4 实现** | `NativeFileDialogService`（NFD UTF-8 API） |
| **E4 门面** | `FileDialogService` — `Get()` / `Initialize` / `Shutdown`；由 **Engine** 在 `StartSystems` 创建 |
| **Third-Party** | `nativefiledialog-extended` submodule → **`minEngine` 目标** 链 `nfd` |
| **Filter 桥接** | `AssetTypeRegistry::BuildFileDialogFilters()` → `std::vector<FileDialogFilter>` |
| **Editor 消费** | `IEditorContext::GetFileDialogService()` **转发** `FileDialogService::Get()` |
| **验收** | Editor Debug/菜单：Open / Save / SelectFolder；filter 含各资产类型 |

### 1.2 不包含（P4+）

- `AssetWorkflowModule::ImportAssetDialog`（P4）
- `DeleteSelectedAsset` / Content Browser（P4/P6）
- `ProjectAssetWatcher` / efsw（P5）
- GLFW **parent window** 置顶（P3.1；v0 `parent = nullptr`）
- `MINENGINE_ENABLE_FILE_DIALOG` 编译开关（v0 始终启用；无头服务器以后再加）
- Font 类型 FileDialog filter（appearance 注册类型后补）

---

## 2) 修改与新建文件

| 文件 | 动作 |
|------|------|
| `Runtime/Platform/FileDialog/FileDialogTypes.h` | **新建** — Filter / Request / Result |
| `Runtime/Platform/FileDialog/IFileDialogService.h` | **新建** |
| `Runtime/Platform/FileDialog/NativeFileDialogService.h` | **新建** |
| `Runtime/Platform/FileDialog/NativeFileDialogService.cpp` | **新建** — NFD 实现 |
| `Runtime/Platform/FileDialog/FileDialogService.h` | **新建** — 门面 + `Get()` |
| `Runtime/Platform/FileDialog/FileDialogService.cpp` | **新建** |
| `Runtime/Engine.h` / `Engine.cpp` | **修改** — `StartSystems` / `ShutdownSystems` 挂接 `FileDialogService` |
| `Runtime/Resource/AssetTypeRegistry.h` | **修改** — `BuildFileDialogFilters()` |
| `Runtime/Resource/AssetTypeRegistry.cpp` | **修改** — 实现（`#include` Platform `FileDialogTypes.h`） |
| `minEngine/minEngine/CMakeLists.txt` | **修改** — `add_subdirectory(nfd)` + `target_link_libraries(minEngine PRIVATE nfd)` |
| `minEngine/Third-Party/nativefiledialog-extended/` | **submodule**（用户审批后添加，§9） |
| `Editor/src/Shell/IEditorContext.h` | **修改** — `GetFileDialogService()` 转发 |
| `Editor/src/Editor.cpp` | **修改** — 实现转发（不持有 `unique_ptr` 实现） |
| `Editor/.../MainMenuModule.cpp`（或 Debug） | **修改** — P3 验收；`BuildFileDialogFilters` + `GetFileDialogService()` |

**不修改：** `AssetManager` CRUD、`UI/Property/**`、`Color.*`、`AssetWorkflowModule` 导入业务（P4）

**明确不要新建：** `Editor/src/Services/FileDialog/`（避免 Editor 重复实现）

---

## 3) `FileDialogTypes.h` + `IFileDialogService.h`（定稿）

路径：`minEngine/minEngine/src/Runtime/Platform/FileDialog/`

```cpp
// FileDialogTypes.h
#pragma once
#include "Core.h"
#include <filesystem>
#include <string>
#include <vector>

namespace minEngine
{
    struct FileDialogFilter
    {
        std::string Label;           // NFD friendly name, e.g. "Material (*.memtl)"
        std::string ExtensionSpec;   // NFD spec: "memtl" or "png,jpg" (no dots)
    };

    struct FileDialogRequest
    {
        std::string Title;
        std::filesystem::path InitialDirectory;
        std::vector<FileDialogFilter> Filters;
        bool bAllowMultiple = false;
    };

    struct FileDialogResult
    {
        bool bCancelled = true;
        std::vector<std::filesystem::path> Paths;
    };
}
```

```cpp
// IFileDialogService.h
#pragma once
#include "FileDialogTypes.h"
#include <string_view>

namespace minEngine
{
    class IFileDialogService
    {
    public:
        virtual ~IFileDialogService() = default;

        virtual FileDialogResult OpenFiles(const FileDialogRequest& request) = 0;
        virtual FileDialogResult SaveFile(
            const FileDialogRequest& request,
            std::string_view defaultFileName = {}) = 0;
        virtual FileDialogResult SelectFolder(const FileDialogRequest& request) = 0;
    };
}
```

| 约定 | 说明 |
|------|------|
| `Filters` 为空 | NFD `filterCount = 0`（全部文件） |
| `Paths` | 绝对路径；取消时为空 |
| 错误 | `ME_CORE_ERROR` + 返回 cancelled（v0 无异常） |

---

## 4) `FileDialogService` — Engine 级门面

与 `AssetManager` / `ProjectManager` 同模式：

```cpp
// FileDialogService.h
class FileDialogService
{
public:
    static FileDialogService& Get();
    static bool HasInstance();

    void Initialize();
    void Shutdown();

    IFileDialogService& GetImplementation();
    const IFileDialogService& GetImplementation() const;

private:
    friend class Engine;
    static void SetInstance(FileDialogService* instance);

    std::unique_ptr<IFileDialogService> m_Implementation;
    static FileDialogService* s_Instance;
};
```

| 阶段 | 行为 |
|------|------|
| `Initialize()` | `NFD_Init()`；失败则 `ME_CORE_ERROR`，后续调用视为 cancelled |
| `Shutdown()` | `NFD_Quit()`；`m_Implementation.reset()` |
| 构造实现 | `m_Implementation = std::make_unique<NativeFileDialogService>()` |

**Engine 挂钩（定稿顺序）：**

```text
StartSystems():
  … ObjectManager, ProjectManager, AssetManager …
  FileDialogService::SetInstance(m_FileDialogService.get());
  m_FileDialogService->Initialize();

ShutdownSystems():
  m_FileDialogService->Shutdown();
  FileDialogService::SetInstance(nullptr);
```

`Engine` 持有 `std::shared_ptr<FileDialogService> m_FileDialogService`（与其它 Manager 一致）。

**Playground / 游戏（P3 后可用，非 P3 验收项）：**

```cpp
FileDialogService::Get().GetImplementation().OpenFiles(request);
```

---

## 5) `NativeFileDialogService` 行为

### 5.1 NFD API

| 操作 | NFD（UTF-8） |
|------|----------------|
| Open 单选 | `NFD_OpenDialogU8_With` |
| Open 多选 | `NFD_OpenDialogMultipleU8_With` |
| Save | `NFD_SaveDialogU8_With` |
| Folder | `NFD_PickFolderU8_With` |

### 5.2 参数映射

```text
request.InitialDirectory  → args.defaultPath
request.Filters           → nfdu8filteritem_t[]（由 ExtensionSpec 填充 spec 字段）
request.bAllowMultiple    → 仅 OpenFiles
defaultFileName           → args.defaultName（Save）
parent window (v0)        → nullptr
```

### 5.3 生命周期

- **仅** `FileDialogService::Initialize/Shutdown` 调用 `NFD_Init` / `NFD_Quit`。
- `NativeFileDialogService` **不** Init/Quit。

---

## 6) `AssetTypeRegistry` — Filter 桥接（Resource → Platform 类型）

```cpp
// AssetTypeRegistry.h — 需前向 include 或 include FileDialogTypes.h
std::vector<FileDialogFilter> BuildFileDialogFilters() const;
```

| 源 | 目标 |
|----|------|
| `FileDialogFilterLabel` | `Label` |
| `Extensions` | `ExtensionSpec`：去 `.`、小写、`,` 连接 |

**合成项（v0）：** `All recognized assets` + 扩展名并集。

保留 `BuildFileDialogFilterSpec()`（仅 label 字符串，日志/兼容）。

**禁止：** `AssetTypeRegistry` 调用 `IFileDialogService`（Resource 不打开对话框）。

---

## 7) Editor 消费（薄层）

```cpp
// IEditorContext.h
IFileDialogService& GetFileDialogService();
const IFileDialogService& GetFileDialogService() const;
```

```cpp
// Editor.cpp
IFileDialogService& Editor::GetFileDialogService()
{
    return FileDialogService::Get().GetImplementation();
}
```

**P3 验收示例（MainMenu / Debug）：**

```cpp
FileDialogRequest request;
request.Title = "Open Asset Files";
request.Filters = AssetTypeRegistry::Get().BuildFileDialogFilters();
request.bAllowMultiple = true;
request.InitialDirectory = PathRegistry::Get().GetProjectContentRoot();

FileDialogResult result =
    context.GetFileDialogService().OpenFiles(request);
```

**不**在 Editor 再建 `m_FileDialogService` 实现实例。

---

## 8) P3 验收（不含 Import）

| # | 检查 |
|---|------|
| 1 | `Engine` 启停后无 NFD 错误；重复打开对话框正常 |
| 2 | Open 多选 / Cancel 行为正确 |
| 3 | Save 带 `defaultFileName` |
| 4 | Select Folder 返回绝对路径 |
| 5 | Filter 含 Material / Mesh / Scene 等 |
| 6 | **`libminEngine` 链接 `nfd`**；Editor 仅链 `minEngine`（不重复链 `nfd`） |
| 7 | Windows 构建通过即可合 P3 |

---

## 9) NFD submodule（用户审批后执行）

| 项 | 值 |
|----|-----|
| 上游 | https://github.com/btzy/nativefiledialog-extended |
| Tag | `v1.3.0` |
| 路径 | `minEngine/minEngine/Third-Party/nativefiledialog-extended/` |

```bash
git submodule add https://github.com/btzy/nativefiledialog-extended.git minEngine/minEngine/Third-Party/nativefiledialog-extended
cd minEngine/minEngine/Third-Party/nativefiledialog-extended
git checkout v1.3.0
```

### CMake（Runtime，定稿）

在 `minEngine/minEngine/CMakeLists.txt`（`${ENGINE_NAME}` 定义之后）：

```cmake
set(NFD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Third-Party/nativefiledialog-extended)
add_subdirectory(${NFD_DIR} ${CMAKE_BINARY_DIR}/third-party/nfd EXCLUDE_FROM_ALL)
target_link_libraries(${ENGINE_NAME} PRIVATE nfd)
target_include_directories(${ENGINE_NAME} PUBLIC
    ./src/Runtime/Platform/FileDialog
)
```

| 选项 | 值 |
|------|-----|
| `NFD_BUILD_TESTS` | OFF |
| `BUILD_SHARED_LIBS` | OFF |

**Editor/CMakeLists.txt：** 无需 `target_link_libraries(Editor PRIVATE nfd)`（经 `minEngine` 传递即可，除非链接器要求显式 — 实现时以 MinGW 实际为准）。

### 平台依赖

| 平台 | P3 最低 |
|------|---------|
| Windows | ✅ 主路径 |
| Linux GTK / portal | 后续；不阻塞 Windows PR |

---

## 10) 实施顺序（P3 内）

| 切片 | 内容 |
|------|------|
| **P3a** | submodule + `minEngine` CMake 链 `nfd` |
| **P3b** | Platform 类型 + `IFileDialogService` + `NativeFileDialogService` |
| **P3c** | `FileDialogService` + `Engine` 生命周期 |
| **P3d** | `BuildFileDialogFilters` + Editor 验收入口 |

建议 **一个 commit**：`feat(platform): Runtime FileDialog service with NFD`

---

## 11) 审批清单

- [ ] **A.** 同意 P3 范围（Platform + Engine + Editor 转发 + 菜单验收）
- [ ] **B.** 同意 `FileDialogFilter::ExtensionSpec` 格式（逗号、无点）
- [ ] **C.** 同意 `NFD_Init`/`Quit` 由 **`FileDialogService` / Engine** 拥有（非 Editor 独占）
- [ ] **D.** 同意 v0 `parent window = nullptr`（P3.1）
- [ ] **E.** 同意 submodule 路径；**`nfd` 链 `minEngine` Runtime**
- [ ] **F.** 同意 Resource 仅产 filter，不依赖 `IFileDialogService` 实现

批准后回复 **「P3 API 批准」** → 你先加 submodule（§9），或交给 agent 实现 P3a–P3d。

---

## 12) 审批后文档

- 更新 [ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md) §2、§6、§9  
- [ASSET_PIPELINE_P2_API.md](./ASSET_PIPELINE_P2_API.md) §10 指向本文
