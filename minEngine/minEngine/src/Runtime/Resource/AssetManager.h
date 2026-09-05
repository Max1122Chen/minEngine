#pragma once
#include "Core.h"
#include "Core/TypeTraits.h"
#include "Runtime/Core/Math/Math.h"
#include "AssetMeta.h"
#include "Asset.h"
#include "AssetRegistryTypes.h"
#include "AssetRegistry.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace minEngine::Reflection
{
    class MEClass;
}

namespace minEngine
{
    class Engine;
    class MEObject;
    class Texture2D;
    class StaticMesh;
    class SkeletalMesh;
    class Skeleton;
    class Material;
    class Scene;
    class Font;
    class LuaScript;
    class EnvironmentMap;
    class AudioClip;
    class AnimationClip;
    class Asset;
    class AssetManager;

    struct ImportAssetResult
    {
        bool bSuccess = false;
        std::string ErrorMessage;
        AssetMeta Meta;
    };

    // ASSET-F02: untyped Load dispatch by AssetTypeId (replaces if-chain in AssetManager).
    using AssetLoadHandlerFn = std::shared_ptr<Asset> (*)(
        AssetManager& manager,
        const AssetMeta& meta,
        std::string& outErrorMessage);

    struct ImportRequest
    {
        std::filesystem::path SourcePath;
        std::filesystem::path DestDirectory;
        std::string ProductId;
        std::string SkeletonAssetPath;
        int AnimationIndex = 0;
        // When true, overwrite an existing cooked product and keep its meta Guid.
        bool bOverwriteExisting = false;
    };

    struct ImportCreatedAsset
    {
        std::string AssetPath;
        std::string AssetTypeId;
        GUID Guid;
    };

    struct ImportResult
    {
        bool bSuccess = false;
        std::string ErrorMessage;
        std::vector<ImportCreatedAsset> Created;
        std::vector<std::string> Warnings;
    };

    struct ImportProductDescriptor
    {
        std::string ProductId;
        std::string DisplayName;
        bool (*AcceptsSourceExtension)(std::string_view extension) = nullptr;
        bool bNeedsSkeletonPicker = false;
        ImportResult (*Import)(AssetManager& manager, const ImportRequest& request) = nullptr;
    };

    class AssetManager
    {
    public:
        AssetManager() = default;
        ~AssetManager() = default;

        static AssetManager& Get();

        void Initialize();
        void Shutdown();

        // Register before untyped Load (typically from Loader pipeline bootstrap).
        void RegisterLoadHandler(std::string_view assetTypeId, AssetLoadHandlerFn handler);
        AssetLoadHandlerFn FindLoadHandler(std::string_view assetTypeId) const;

        void RegisterImportProduct(const ImportProductDescriptor& descriptor);
        const ImportProductDescriptor* FindImportProduct(std::string_view productId) const;
        const std::vector<ImportProductDescriptor>& GetImportProducts() const { return m_ImportProducts; }

        // ASSET-F02: dispatch by registered ProductId.
        ImportResult Import(const ImportRequest& request);

        // Re-cook from meta.SourcePath; overwrites product in place and keeps Guid.
        bool Reimport(const std::string& assetPath, std::string& outError);

        void ScanAssets(const std::filesystem::path& directory);
        AssetMeta RegisterAsset(const std::string& path, const std::string& assetTypeId);

        bool DeleteAsset(const std::string& assetPath, std::string& outError);
        bool MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError);
        bool RenameAsset(const std::string& oldPath, const std::string& newFileName, std::string& outError);
        bool UnregisterAsset(const std::string& assetPath, std::string& outError);
        bool RemoveMetaFileOnDisk(const std::string& assetPath, std::string& outError);

        // Write .meta before RegisterAsset so Guid stays stable (matches serialized MEObject::m_Guid).
        bool WriteOrUpdateMetaFile(const AssetMeta& meta);

        // After asset deserialize, force identity from registry meta (file may embed a stale m_Guid).
        void ApplyMetaIdentity(MEObject& object, const AssetMeta& meta);

        void ClearProjectRegistry();

        class AssetRegistryBroadcastBatchScope
        {
        public:
            AssetRegistryBroadcastBatchScope();
            ~AssetRegistryBroadcastBatchScope();

            AssetRegistryBroadcastBatchScope(const AssetRegistryBroadcastBatchScope&) = delete;
            AssetRegistryBroadcastBatchScope& operator=(const AssetRegistryBroadcastBatchScope&) = delete;

        private:
            bool m_Active = false;
        };

        std::shared_ptr<Asset> LoadAssetByGUID(const GUID& guid, std::string& outErrorMessage);
        std::shared_ptr<Asset> LoadAssetByPath(const std::string& path, std::string& outErrorMessage);
        std::shared_ptr<Asset> LoadAssetByMeta(const AssetMeta& meta, std::string& outErrorMessage);

        const AssetMeta* FindAssetMetaByPath(const std::string& path) const;
        const AssetMeta* FindAssetMetaByGuid(const GUID& guid) const;
        std::vector<const AssetMeta*> FindAssetMetasByType(const std::string& assetTypeId) const;
        std::vector<const AssetMeta*> FindAssetMetasByClass(const Reflection::MEClass* assetClass) const;
        std::vector<const AssetMeta*> FindAssetMetasByRuntimeClass(const std::string& runtimeClassName) const;
        std::vector<const AssetMeta*> FindAssetMetasUnderDirectory(
            std::string_view projectRelativeDirectory) const;

        uint32_t Subscribe(AssetRegistryChangedCallback callback);
        void Unsubscribe(uint32_t subscriptionId);

        std::filesystem::path ResolveAssetAbsolutePath(std::string_view projectRelativeOrLegacyPath) const;

        static bool HasInstance();

        void MarkReachableLoadedAssets(const std::function<void(MEObject*)>& markReachable) const;

        template<typename T>
        std::shared_ptr<T> LoadAsset(const std::string& path)
        {
            if constexpr (!std::is_base_of_v<Asset, T>)
            {
                static_assert(AlwaysFalse<T>::value, "LoadAsset<T> is only implemented for types derived from Asset");
                return nullptr;
            }

            const std::string registryKey = NormalizeProjectRelativeAssetPath(path);
            if (registryKey.empty())
            {
                return nullptr;
            }

            if (m_LoadedAssetCache.find(registryKey) != m_LoadedAssetCache.end())
            {
                std::shared_ptr<MEObject> cachedAsset = std::static_pointer_cast<MEObject>(m_LoadedAssetCache[registryKey].lock());
                if (cachedAsset)
                {
                    return std::dynamic_pointer_cast<T>(cachedAsset);
                }
            }

            const AssetMeta* meta = FindAssetMetaByPath(registryKey);
            if (meta == nullptr)
            {
                return nullptr;
            }

            std::shared_ptr<T> asset = LoadAsset_Impl<T>(*meta);
            if (asset)
            {
                m_LoadedAssetCache[registryKey] = asset;
                std::shared_ptr<Asset> genericAsset = std::static_pointer_cast<Asset>(asset);
                genericAsset->SetMeta(const_cast<AssetMeta*>(meta));
            }

            return asset;
        }

        template<typename T>
        bool SaveAsset(const std::string& path, const T& asset) const
        {
            const AssetMeta* meta = FindAssetMetaByPath(path);
            if (meta == nullptr)
            {
                return false;
            }

            return SaveAsset_Impl<T>(*meta, asset);
        }

        template<typename T>
        std::shared_ptr<T> CreateAsset(const std::string& assetName, const std::string& directoryRel);

    private:
        template<typename T>
        std::shared_ptr<T> RemoveAsset(const std::string& path)
        {
            static_assert(AlwaysFalse<T>::value, "RemoveAsset<T> is not implemented for this type T");
            (void)path;
            return nullptr;
        }

        template<typename T>
        std::shared_ptr<T> LoadAsset_Impl(const AssetMeta& meta)
        {
            static_assert(AlwaysFalse<T>::value, "LoadAsset_Impl<T> is not implemented for this type T");
            (void)meta;
            return nullptr;
        }

        template<typename T>
        bool SaveAsset_Impl(const AssetMeta& meta, const T& asset) const
        {
            static_assert(AlwaysFalse<T>::value, "SaveAsset_Impl<T> is not implemented for this type T");
            (void)meta;
            (void)asset;
            return true;
        }

        template<typename T>
        std::shared_ptr<Asset> LoadTypedAssetAsBase(
            const AssetMeta& meta,
            std::string& outErrorMessage,
            const char* failureMessage)
        {
            std::shared_ptr<T> asset = LoadAsset<T>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = failureMessage;
                return nullptr;
            }

            return std::static_pointer_cast<Asset>(asset);
        }

        enum class MeshImportProductType
        {
            StaticMesh,
            SkeletalMesh
        };

        // Private cook implementations used by ImportProduct_* handlers.
        ImportAssetResult ImportAsset(
            const std::filesystem::path& sourcePath,
            const std::filesystem::path& destDirectory,
            bool bOverwriteExisting = false);
        ImportAssetResult ImportExternalMesh(
            const std::filesystem::path& sourcePath,
            const std::filesystem::path& destDirectory,
            MeshImportProductType productType,
            bool bOverwriteExisting = false);
        ImportAssetResult ImportAnimationClip(
            const std::filesystem::path& sourcePath,
            const std::filesystem::path& destDirectory,
            std::string_view skeletonAssetPath,
            int animationIndex = 0,
            bool bOverwriteExisting = false);

        static std::shared_ptr<Asset> LoadHandler_StaticMesh(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_SkeletalMesh(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Skeleton(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_AnimationClip(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Texture2D(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Scene(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Material(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Font(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_LuaScript(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_EnvironmentMap(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_AudioClip(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_ShaderRemoved(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);

        static bool AcceptsNativeCopyExtension(std::string_view extension);
        static bool AcceptsExternalMeshCookExtension(std::string_view extension);
        static bool AcceptsAnimationClipSourceExtension(std::string_view extension);

        static ImportResult ImportProduct_NativeCopy(AssetManager& manager, const ImportRequest& request);
        static ImportResult ImportProduct_StaticMesh(AssetManager& manager, const ImportRequest& request);
        static ImportResult ImportProduct_SkeletalMesh(AssetManager& manager, const ImportRequest& request);
        static ImportResult ImportProduct_AnimationClip(AssetManager& manager, const ImportRequest& request);

        static ImportResult MakeImportResultFromLegacy(const ImportAssetResult& legacy);

        friend void RegisterAllAssetPipelines(AssetManager& assetManager);
        friend class Engine;
        friend class AssetManagerTestScope;
        friend class LuaScriptMvpTestScope;

        static void SetInstance(AssetManager* instance);
        static AssetManager* s_Instance;

        std::shared_ptr<Asset> LoadAssetByMeta_Internal(const AssetMeta& meta, std::string& outErrorMessage);

        std::string NormalizeProjectRelativeAssetPath(const std::string& path) const;
        std::filesystem::path BuildMetaAbsolutePath(std::string_view projectRelativeAssetPath) const;
        bool IsUnderProjectContentRoot(const std::filesystem::path& absolutePath) const;
        bool IsUnderEngineDefaultAssetsRoot(const std::filesystem::path& absolutePath) const;

        void CacheMeta(const AssetMeta& meta, bool alreadyRegistered);
        void UncacheMeta(std::string_view projectRelativePath);

        void EvictLoadedAssetCache(std::string_view projectRelativePath);
        void MoveLoadedAssetCacheKey(std::string_view oldRel, std::string_view newRel);
        bool LogReferenceWarningsForDelete(const AssetMeta& meta) const;
        bool MoveRegistryEntry(std::string_view oldRel, std::string_view newRel, AssetMeta& inOutMeta);
        bool WriteMetaFile(const AssetMeta& meta) const;
        std::string ResolveAssetAbsolutePathString(std::string_view projectRelativeOrLegacyPath) const;
        static bool IsPathInsideDirectory(
            const std::filesystem::path& absolutePath,
            const std::filesystem::path& rootDirectory);

        void BeginRegistryBroadcastBatch();
        void EndRegistryBroadcastBatch();
        void NoteEditorFilesystemMutation(const std::filesystem::path& absolutePath) const;

        void RemoveOrphanMetaFilesInDirectory(const std::filesystem::path& directory);

        AssetRegistry m_Registry;
        std::unordered_map<std::string, std::weak_ptr<MEObject>> m_LoadedAssetCache;
        std::unordered_map<std::string, AssetLoadHandlerFn> m_LoadHandlers;
        std::vector<ImportProductDescriptor> m_ImportProducts;
        int m_RegistryBroadcastBatchDepth = 0;
    };

    template<>
    std::shared_ptr<Scene> AssetManager::LoadAsset_Impl<Scene>(const AssetMeta& meta);
    template<>
    std::shared_ptr<StaticMesh> AssetManager::LoadAsset_Impl<StaticMesh>(const AssetMeta& meta);
    template<>
    std::shared_ptr<SkeletalMesh> AssetManager::LoadAsset_Impl<SkeletalMesh>(const AssetMeta& meta);
    template<>
    std::shared_ptr<Skeleton> AssetManager::LoadAsset_Impl<Skeleton>(const AssetMeta& meta);
    template<>
    std::shared_ptr<Texture2D> AssetManager::LoadAsset_Impl<Texture2D>(const AssetMeta& meta);
    template<>
    std::shared_ptr<Material> AssetManager::LoadAsset_Impl<Material>(const AssetMeta& meta);
    template<>
    std::shared_ptr<Font> AssetManager::LoadAsset_Impl<Font>(const AssetMeta& meta);
    template<>
    std::shared_ptr<LuaScript> AssetManager::LoadAsset_Impl<LuaScript>(const AssetMeta& meta);
    template<>
    std::shared_ptr<EnvironmentMap> AssetManager::LoadAsset_Impl<EnvironmentMap>(const AssetMeta& meta);
    template<>
    std::shared_ptr<AudioClip> AssetManager::LoadAsset_Impl<AudioClip>(const AssetMeta& meta);
    template<>
    std::shared_ptr<AnimationClip> AssetManager::LoadAsset_Impl<AnimationClip>(const AssetMeta& meta);

    template<>
    bool AssetManager::SaveAsset_Impl<Scene>(const AssetMeta& meta, const Scene& asset) const;
    template<>
    bool AssetManager::SaveAsset_Impl<Material>(const AssetMeta& meta, const Material& asset) const;

    template<>
    std::shared_ptr<Scene> AssetManager::CreateAsset<Scene>(
        const std::string& assetName,
        const std::string& directoryRel);
    template<>
    std::shared_ptr<Material> AssetManager::CreateAsset<Material>(
        const std::string& assetName,
        const std::string& directoryRel);
}
