# Asset Pipeline — P1 接口定稿（审批用）

Last updated: 2026-05-25  
Status: **已批准 — P1/P1b 实现中/已完成见 PROGRESS_LOG**  
父文档：[ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md)（§10、§14 已拍板）  
范围：**P1 + P1b**（同一次实现提交，接口一并冻结）

---

## 1) P1 交付边界

### 1.1 包含

| 项 | 说明 |
|----|------|
| **R0** | 新文件 `AssetTypeRegistry.{h,cpp}`，内置类型表，供 Scan / Import /（未来）FileDialog |
| **类型桶** | `m_AssetMetasByType`，`CacheMeta` / `UncacheMeta` 维护 |
| **变更事件** | `AssetRegistryChangeKind`、`Subscribe` / `Unsubscribe`；`RegisterAsset` / `ImportAsset` 成功发 `Registered` |
| **ImportAsset** | 单文件复制进工程 `Assets/` + `RegisterAsset` |
| **P1b 相对路径** | `AssetMeta.AssetPath` 与 Registry 键为 **工程相对路径**；IO 前解析为绝对路径 |
| **P1b Editor** | 移除 `Editor.cpp` 对 `EngineDefaultAssetsRoot` 的 `ScanAssets` |
| **查询** | `FindAssetMetasByType` O(1) 桶；**破坏性**返回 `const AssetMeta*` |

### 1.2 不包含（P2+）

- `DeleteAsset` / `MoveAsset`
- `IFileDialogService` / NFD
- `ProjectAssetWatcher` / efsw
- `ContentBrowserModule`
- `ProjectTemplate::SeedDefaultAssets`（仅文档预留）
- `ForEachAssetMetaOfType`（P1 可省略，需要时 P2 加）
- Font 类型注册（appearance 线）

---

## 2) 新增与修改文件

| 文件 | 动作 |
|------|------|
| `Runtime/Resource/AssetRegistryTypes.h` | **新建** — 事件枚举与 payload |
| `Runtime/Resource/AssetTypeRegistry.h` | **新建** |
| `Runtime/Resource/AssetTypeRegistry.cpp` | **新建** |
| `Runtime/Resource/AssetManager.h` | **修改** — 公开 API + 私有成员 |
| `Runtime/Resource/AssetManager.cpp` | **修改** — 实现 |
| `minEngine/CMakeLists.txt`（或 Resource 子 CMake） | 加入新 .cpp |
| `Editor/src/Editor.cpp` | **修改** — 删除 EngineDefault `ScanAssets`（P1b） |
| `Editor/.../MaterialEditor.cpp` | **修改** — `FindAssetMetasByType` 返回值/参数 |
| `Editor/.../MaterialNodeDefPropertyDrawer.cpp` | **修改** — 同上（非 `UI/Property/`） |
| `Editor/.../SceneEditorInspectorSource.cpp` | **修改** — 同上 |
| `MyMEProject/Assets/**/*.meta`（及 golden） | **修改** — `AssetPath` 改为相对（P1b 验收） |

**不修改：** `Editor/src/UI/Property/**`、`Color.*`、`EDITOR_APPEARANCE.md`

---

## 3) `AssetRegistryTypes.h`

```cpp
#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

#include <cstdint>
#include <functional>
#include <string>

namespace minEngine
{
    enum class AssetRegistryChangeKind : uint8_t
    {
        Registered   = 0,
        Unregistered = 1,
        Moved        = 2,  // P2; P1 不发射
        MetaUpdated    = 3,  // P1: RegisterAsset 更新已有 meta 时
        Reimported     = 4   // P2+; P1 不发射
    };

    struct AssetRegistryChange
    {
        AssetRegistryChangeKind Kind = AssetRegistryChangeKind::Registered;
        GUID Guid{};
        std::string OldPath;     // 工程相对路径；Unregistered/Moved 时有效
        std::string NewPath;     // 工程相对路径；Registered/Moved/MetaUpdated 时有效
        std::string AssetTypeId;
    };

    using AssetRegistryChangedCallback = std::function<void(const AssetRegistryChange&)>;

    /** Invalid subscription id; never passed to Unsubscribe. */
    constexpr uint32_t kInvalidAssetRegistrySubscriptionId = 0u;
}
```

---

## 4) `AssetTypeRegistry.h`

```cpp
#pragma once

#include "Core.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    struct AssetTypeDescriptor
    {
        std::string AssetTypeId;
        std::string RuntimeClassName;
        std::vector<std::string> Extensions;
        std::string FileDialogFilterLabel;
    };

    class AssetTypeRegistry
    {
    public:
        static AssetTypeRegistry& Get();

        void RegisterBuiltinTypes();

        /** 运行期扩展（如 appearance 注册 Font）；扩展名小写含 '.'。 */
        void RegisterType(const AssetTypeDescriptor& descriptor);

        const AssetTypeDescriptor* FindByExtension(std::string_view extension) const;
        const AssetTypeDescriptor* FindByAssetTypeId(std::string_view assetTypeId) const;

        std::string InferAssetTypeFromExtension(const std::filesystem::path& path) const;
        std::string InferAssetTypeFromRuntimeClassName(std::string_view runtimeClassName) const;

        /** 供 P3 NFD；P1 可实现为空 vector 或先实现 filter 字符串。 */
        std::vector<std::string> BuildFileDialogFilterSpec() const;

        const std::vector<AssetTypeDescriptor>& GetDescriptors() const;

    private:
        std::vector<AssetTypeDescriptor> m_Descriptors;
    };
}
```

**Builtin 表（P1）：**

| AssetTypeId | RuntimeClassName | Extensions |
|-------------|------------------|------------|
| Texture2D | `Reflection::GetClassName<Texture2D>()` | `.png`, `.jpg`, `.jpeg` |
| StaticMesh | `…<StaticMesh>()` | `.obj`, `.fbx`, `.gltf` |
| Material | `…<Material>()` | `.memtl` |
| Shader | `…<Shader>()` | `.meshader` |
| Scene | `…<Scene>()` | `.mescene` |

**初始化时机：** `AssetManager::Initialize()` 内调用 `AssetTypeRegistry::Get().RegisterBuiltinTypes()`（仅一次）。

---

## 5) `AssetManager.h` — P1 公开 API（定稿）

### 5.1 结果类型

```cpp
struct ImportAssetResult
{
    bool bSuccess = false;
    std::string ErrorMessage;
    AssetMeta Meta;
};
```

### 5.2 类声明（公开部分；模板成员保持现有位置）

```cpp
class AssetManager
{
public:
    AssetManager() = default;
    ~AssetManager() = default;

    static AssetManager& Get();
    static bool HasInstance();

    void Initialize();
    void Shutdown();

    // --- Scan / Register (existing, semantics extended) ---
    void ScanAssets(const std::filesystem::path& directory);
    AssetMeta RegisterAsset(const std::string& path, const std::string& assetTypeId);

    // --- P1 Import ---
    ImportAssetResult ImportAsset(const std::filesystem::path& sourcePath,
                                  const std::filesystem::path& destDirectory);

    // --- Load (unchanged signatures; path = project-relative or legacy absolute) ---
    std::shared_ptr<Asset> LoadAssetByGUID(const GUID& guid, std::string& outErrorMessage);
    std::shared_ptr<Asset> LoadAssetByPath(const std::string& path, std::string& outErrorMessage);
    std::shared_ptr<Asset> LoadAssetByMeta(const AssetMeta& meta, std::string& outErrorMessage);

    // --- Query ---
    const AssetMeta* FindAssetMetaByPath(const std::string& path) const;
    const AssetMeta* FindAssetMetaByGuid(const GUID& guid) const;

    /** @param assetTypeId 必须为 AssetTypeId（如 "Material"），非任意显示名。 */
    std::vector<const AssetMeta*> FindAssetMetasByType(const std::string& assetTypeId) const;

    /**
     * Editor 便利 API：入参为 Reflection 运行时类名，内部映射到 AssetTypeId 再查桶。
     * 行为等价于现 FindAssetMetasByType + InferAssetTypeFromClassName。
     */
    std::vector<const AssetMeta*> FindAssetMetasByRuntimeClass(const std::string& runtimeClassName) const;

    // --- P1 Events ---
    uint32_t Subscribe(AssetRegistryChangedCallback callback);
    void Unsubscribe(uint32_t subscriptionId);

    // --- P1b Path ---
    /** 将 meta 或查询用的 path 转为磁盘绝对路径；依赖 PathRegistry::GetProjectContentRoot()。 */
    std::filesystem::path ResolveAssetAbsolutePath(std::string_view projectRelativeOrLegacyPath) const;

    void MarkReachableLoadedAssets(const std::function<void(MEObject*)>& markReachable) const;

    template<typename T>
    std::shared_ptr<T> LoadAsset(const std::string& path);  // 实现改用 Resolve + 相对键

    template<typename T>
    bool SaveAsset(const std::string& path, const T& asset) const;

private:
    // ... existing template stubs ...

    std::shared_ptr<Asset> LoadAssetByMeta_Internal(const AssetMeta& meta, std::string& outErrorMessage);

    /** 输出：工程相对路径（POSIX '/'）；失败返回空串。 */
    std::string NormalizeProjectRelativeAssetPath(const std::string& path) const;

    std::filesystem::path BuildMetaAbsolutePath(const AssetMeta& meta) const;
    std::filesystem::path BuildMetaAbsolutePathForRelative(std::string_view projectRelativeAssetPath) const;

    bool IsUnderProjectContentRoot(const std::filesystem::path& absolutePath) const;
    bool IsUnderEngineDefaultAssetsRoot(const std::filesystem::path& absolutePath) const;

    void CacheMeta(const AssetMeta& meta);
    void UncacheMeta(std::string_view projectRelativePath);
    void BroadcastChange(const AssetRegistryChange& change);

    static void SetInstance(AssetManager* instance);
    static AssetManager* s_Instance;

    std::unordered_map<std::string, AssetMeta> m_AssetRegistry;
    std::unordered_map<GUID, std::string, GUID::Hash> m_AssetPathByGuid;
    std::unordered_map<std::string, std::vector<AssetMeta*>> m_AssetMetasByType;
    std::unordered_map<std::string, std::weak_ptr<MEObject>> m_LoadedAssetCache;

    std::unordered_map<uint32_t, AssetRegistryChangedCallback> m_Subscribers;
    uint32_t m_NextSubscriptionId = 1u;
};
```

**移除（私有，改由 R0 承担）：**

- `InferAssetTypeFromExtension` / `InferAssetTypeFromClassName` 成员 → 委托 `AssetTypeRegistry`
- `NormalizeAssetPath` → 由 `NormalizeProjectRelativeAssetPath` + `ResolveAssetAbsolutePath` 替代

---

## 6) 行为契约（实现必须满足）

### 6.1 路径（P1b）

| 规则 | 说明 |
|------|------|
| 存储 | `AssetMeta.AssetPath` 与 `m_AssetRegistry` 键均为 **相对** `ProjectContentRoot` 的路径，如 `Materials/Foo.memtl` |
| 规范化 | `/` 分隔；无 leading `/`；禁止 `..` 逃出 ContentRoot |
| 入参 | `FindAssetMetaByPath` / `LoadAsset(path)` / `RegisterAsset(path)` 接受 **相对或绝对**；内部统一规范化成相对键 |
| 迁移 | 读 meta 时若为绝对且位于当前 `ProjectContentRoot` 下 → 转相对并写回 meta |
| 拒绝 | 绝对路径位于 `EngineDefaultAssetsRoot` 下 → **不注册**，`ME_CORE_WARN`，提示应复制到工程 |
| IO | 所有 `filesystem` / `Serializer::FromFile` 使用 `ResolveAssetAbsolutePath` |

### 6.2 `ScanAssets`

- 仅用于 **已存在目录** 递归；行为与现逻辑相同，但：
  - 类型推断 → `AssetTypeRegistry`
  - 注册路径 → 相对化
  - 每条新注册成功 → `Registered`（已存在且 meta 修正 → `MetaUpdated`）

### 6.3 `RegisterAsset`

| 步骤 | 说明 |
|------|------|
| 规范化 | `NormalizeProjectRelativeAssetPath` |
| 空路径 / 越界 | 返回默认构造 `AssetMeta`，不发事件 |
| meta 文件 | 逻辑保持：读 sidecar / 生成 GUID / 碰撞重生 |
| 写 meta | `AssetPath` 字段写 **相对** 路径 |
| 缓存 | `CacheMeta` 更新桶 |
| 事件 | 新 path → `Registered`；已存在 → `MetaUpdated` |
| Scene | `assetTypeId == "Scene"` → `SceneManager::RegisterScene`（与现一致） |

### 6.4 `ImportAsset`

```cpp
ImportAssetResult ImportAsset(sourcePath, destDirectory);
```

| 步骤 | 说明 |
|------|------|
| `sourcePath` | 任意绝对/相对 **现有文件** |
| `destDirectory` | 绝对或相对；必须解析到 **ProjectContentRoot 子目录** |
| 类型 | `AssetTypeRegistry::InferAssetTypeFromExtension(source)`；空 → 失败 |
| 复制 | `copy_file` → `destDirectory / source.filename()` |
| 重名 | 目标已存在 → **失败**（D4），不自动重命名 |
| 注册 | `RegisterAsset(destRelative, assetTypeId)` |
| 事件 | 由 `RegisterAsset` 发 `Registered` |
| 加载 | **不**自动 `LoadAsset`（D11） |

### 6.5 类型桶

- Key = `AssetMeta.AssetType`（= `AssetTypeId`）
- `vector<AssetMeta*>` 指向 `m_AssetRegistry` 内元素；**不拥有**
- `UncacheMeta` 从 vector 中移除对应指针（swap-pop 或 stable_remove）
- `Shutdown` 清空 Registry、桶、订阅者

### 6.6 变更事件

| 项 | 规则 |
|----|------|
| 投递 | **同步**，在 mutator 返回前（D3） |
| 重入 | 回调内 **禁止** 调用 `RegisterAsset` / `ImportAsset` / `ScanAssets` / `UncacheMeta` |
| `Unsubscribe` | 允许在回调内调用 |
| `Change.NewPath` / `OldPath` | 均为 **工程相对** 路径 |
| P1 发射 | `Registered`、`MetaUpdated` only |

### 6.7 `FindAssetMetasByType` / `ByRuntimeClass`

- `FindAssetMetasByType("Material")` → 直接查 `m_AssetMetasByType["Material"]`，O(桶大小)
- `FindAssetMetasByRuntimeClass("Material")` → `InferAssetTypeFromRuntimeClassName` → `FindAssetMetasByType`
- 返回指针指向 Registry 内存储；Registry 变更前有效

### 6.8 `LoadAsset` / 缓存键

- `m_LoadedAssetCache` 键 = **规范化相对路径**（与 Registry 一致）
- `LoadAsset_Impl` 内读盘使用 `ResolveAssetAbsolutePath(meta.AssetPath)`

---

## 7) 破坏性变更与调用点迁移（P1 PR 内一并改）

| 调用点 | 变更 |
|--------|------|
| `FindAssetMetasByType` 返回类型 | `AssetMeta*` → `const AssetMeta*` |
| `MaterialEditor::RefreshMaterialList` | 使用 `FindAssetMetasByType("Material")` 或 `FindAssetMetasByRuntimeClass(GetClassName<Material>())`；`m_MaterialMetas` 改为 `vector<const AssetMeta*>` |
| `MaterialNodeDefPropertyDrawer` | `FindAssetMetasByRuntimeClass(typeName)` |
| `SceneEditorInspectorSource::DrawAssetRef` | 同上 |
| `meta->AssetPath` 传 `LoadAsset` / `LoadAssetByPath` | **无需改**（仍传 meta 内相对路径即可） |
| `Editor.cpp` | 删除 `ScanAssets(GetEngineDefaultAssetsRoot())` |

**不改动** `Editor/src/UI/Property/**`（当前无 `FindAssetMetasByType` 调用）。

---

## 8) CMake

```cmake
# minEngine Runtime Resource 源列表增加：
AssetRegistryTypes.h   # header-only, 可选列入 target_sources 或不列
AssetTypeRegistry.cpp
AssetTypeRegistry.h
# AssetManager.cpp 已存在
```

---

## 9) 验收标准（审批后实现完成即测）

| # | 检查 |
|---|------|
| 1 | `RegisterAsset` / `ScanAssets` 后 `FindAssetMetasByType("Material")` 不扫全表 |
| 2 | `Subscribe` 在 `ImportAsset` 成功后收到 `Registered`，`NewPath` 为相对路径 |
| 3 | `MaterialIRSmoke.memtl.meta` 中 `AssetPath` 为 `Materials/MaterialIRSmoke.memtl` 形式 |
| 4 | `ImportAsset` 复制到 `Assets/Test/`，重名失败、未知扩展名失败 |
| 5 | EngineDefault 目录文件 **不在** Registry（Editor 启动后 `FindAssetMetaByPath` 引擎模板路径为空） |
| 6 | `cmake --build` Editor + 现有 `--material-ir-test` 或打开工程目视 Material 列表 |

---

## 10) 审批清单

请逐项确认后回复 **「P1 API 批准」** 或标注修改项：

- [ ] **A.** 同意 P1 与 P1b 同批实现（相对路径 + 去掉 Engine Registry 扫描）
- [ ] **B.** 同意 `FindAssetMetasByType` 仅 `AssetTypeId` + 新增 `FindAssetMetasByRuntimeClass`
- [ ] **C.** 同意返回 `const AssetMeta*` 及 Editor 三处调用点修改
- [ ] **D.** 同意 `AssetRegistryTypes.h` 独立头文件
- [ ] **E.** 同意 `ImportAsset` 语义（重名失败、不自动 Load）
- [ ] **F.** 同意 P1 仅发射 `Registered` / `MetaUpdated` 事件

---

## 11) 审批后下一步

1. 按本文实现 `AssetTypeRegistry` + `AssetManager` P1/P1b  
2. 更新 golden `.meta` 相对路径  
3. `PROGRESS_LOG.md` 记录 P1 完成  
4. P2 接口定稿见 [ASSET_PIPELINE_P2_API.md](./ASSET_PIPELINE_P2_API.md)（待审批）
