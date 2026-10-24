#pragma once
#include "Core.h"
#include "Core/TypeTraits.h"
#include "Runtime/Core/Math/Math.h"
#include "AssetMeta.h"
#include "Asset.h"
#include "AssetRegistryTypes.h"
#include "AssetRegistry.h"
#include "AssetTypeRegistry.h"
#include "Runtime/Core/Serialization/Json.h"

#include <filesystem>
#include <functional>
#include <memory>
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
    class MaterialInstance;
    class Sprite;
    class SpriteAtlas;
    class Scene;
    class Prefab;
    class Font;
    class LuaScript;
    class EnvironmentMap;
    class AudioClip;
    class AnimationClip;
    class AnimationGraph;
    class Asset;
    class MINENGINE_API AssetManager;

    struct ImportAssetResult
    {
        bool bSuccess = false;
        std::string ErrorMessage;
        AssetMeta Meta;
    };

    // untyped Load dispatch by AssetTypeId (replaces if-chain in AssetManager).
    using AssetLoadHandlerFn = std::shared_ptr<Asset> (*)(
        AssetManager& manager,
        const AssetMeta& meta,
        std::string& outErrorMessage);

    // Untyped Save dispatch: the Saver supplies the entry point, so there is no template
    // specialization spine.
    using AssetSaveHandlerFn = bool (*)(
        const AssetManager& manager,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError);

    // Create belongs to Saver and is optional; an empty handler means the type cannot be created.
    using AssetCreateHandlerFn = std::shared_ptr<Asset> (*)(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError);

    struct ImportRequest
    {
        std::filesystem::path SourcePath;
        std::filesystem::path DestDirectory;
        std::string ProductId;
        std::string SkeletonAssetPath;
        int AnimationIndex = 0;
        // Texture2D Product: Color (default) | SingleChannel | Mask. Empty => Color.
        std::string ChannelSemantics;
        // Texture2D Product sampler modes. Empty => Linear / Repeat, so an import that
        // names neither keeps the historical result.
        std::string SamplerFilter;
        std::string SamplerAddress;
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

    /** Opaque handle for AsyncLoadAsset*; zero means rejected / invalid. */
    struct MINENGINE_API AsyncLoadHandle
    {
        uint64_t Id = 0;

        bool IsValid() const { return Id != 0; }

        bool operator==(const AsyncLoadHandle& other) const { return Id == other.Id; }
        bool operator!=(const AsyncLoadHandle& other) const { return Id != other.Id; }
    };

    using AsyncLoadAssetCallback = std::function<void(
        std::shared_ptr<Asset> asset,
        bool bSucceeded,
        std::string_view error)>;

    class AssetManager
    {
    public:
        AssetManager() = default;
        ~AssetManager() = default;

        static AssetManager& Get();

        void Initialize();
        void Shutdown();

        /**
         * Wait accepted DataAsset worker jobs; drop undispatched callbacks (Warn).
         * Call before TaskSystem::Shutdown.
         */
        void FlushAsyncLoadsForShutdown();

        // Register before untyped Load (typically from Loader pipeline bootstrap).
        void RegisterLoadHandler(std::string_view assetTypeId, AssetLoadHandlerFn handler);
        AssetLoadHandlerFn FindLoadHandler(std::string_view assetTypeId) const;

        void RegisterSaveHandler(std::string_view assetTypeId, AssetSaveHandlerFn handler);
        AssetSaveHandlerFn FindSaveHandler(std::string_view assetTypeId) const;

        void RegisterCreateHandler(std::string_view assetTypeId, AssetCreateHandlerFn handler);
        AssetCreateHandlerFn FindCreateHandler(std::string_view assetTypeId) const;

        void RegisterImportProduct(const ImportProductDescriptor& descriptor);
        const ImportProductDescriptor* FindImportProduct(std::string_view productId) const;
        const std::vector<ImportProductDescriptor>& GetImportProducts() const { return m_ImportProducts; }

        // dispatch by registered ProductId.
        ImportResult Import(const ImportRequest& request);

        // Re-cook from meta.SourcePath; overwrites product in place and keeps Guid.
        bool Reimport(const std::string& assetPath, std::string& outError);

        void ScanAssets(const std::filesystem::path& directory);
        // preferredGuid: when non-null and non-zero, seed a new .meta Guid (Create path).
        // Existing on-disk meta still wins; do not silently replace a published Guid.
        AssetMeta RegisterAsset(
            const std::string& path,
            const std::string& assetTypeId,
            const GUID* preferredGuid = nullptr);

        bool DeleteAsset(const std::string& assetPath, std::string& outError);
        /** Prefab: when bUnpackOpenPrefabInstanceRefs, unpack open-editor instances then delete. */
        bool DeleteAsset(
            const std::string& assetPath,
            std::string& outError,
            bool bUnpackOpenPrefabInstanceRefs);
        bool MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError);
        bool RenameAsset(const std::string& oldPath, const std::string& newFileName, std::string& outError);

        /** Create an empty content folder (no .meta). parentRel empty = content root. */
        bool CreateFolder(
            const std::string& parentDirectoryRel,
            const std::string& folderName,
            std::string& outError,
            std::string* outCreatedDirectoryRel = nullptr);
        bool RenameFolder(
            const std::string& directoryRel,
            const std::string& newFolderName,
            std::string& outError);
        /** Default: reject non-empty directories. */
        bool DeleteFolder(
            const std::string& directoryRel,
            std::string& outError,
            bool requireEmpty = true);

        bool UnregisterAsset(const std::string& assetPath, std::string& outError);
        bool RemoveMetaFileOnDisk(const std::string& assetPath, std::string& outError);

        // Write .meta before RegisterAsset so Guid stays stable (matches serialized MEObject::m_Guid).
        bool WriteOrUpdateMetaFile(const AssetMeta& meta) const;

        // After asset deserialize, force identity from registry meta (file may embed a stale m_Guid).
        void ApplyMetaIdentity(MEObject& object, const AssetMeta& meta);

        void ClearProjectRegistry();
        void EvictLoadedAssetCache(std::string_view projectRelativePath);

        class MINENGINE_API AssetRegistryBroadcastBatchScope
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

        /**
         * Async DataAsset load (MVP). Worker: LoadJsonFromFile. Game thread (Drain):
         * NewObject + FromJson (sync nested refs) + cache + callback.
         * Requires BindTaskSystem + Running + game thread.
         */
        AsyncLoadHandle AsyncLoadAssetByPath(const std::string& path, AsyncLoadAssetCallback onComplete);
        AsyncLoadHandle AsyncLoadAssetByGUID(const GUID& guid, AsyncLoadAssetCallback onComplete);
        AsyncLoadHandle AsyncLoadAssetByMeta(const AssetMeta& meta, AsyncLoadAssetCallback onComplete);

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

        // LoadAsset<T> removed — use LoadAssetByPath/ByGUID/ByMeta + cast.

        template<typename T>
        bool SaveAsset(const std::string& path, const T& asset) const
        {
            static_assert(std::is_base_of_v<Asset, T>, "SaveAsset<T> requires T derived from Asset");

            const AssetMeta* meta = FindAssetMetaByPath(path);
            if (meta == nullptr)
            {
                return false;
            }

            return SaveRegisteredAsset(*meta, asset);
        }

        /** Untyped Create via registered Saver.Create (fails if no Save/Create). */
        std::shared_ptr<Asset> CreateAssetByTypeId(
            std::string_view assetTypeId,
            const std::string& assetName,
            const std::string& directoryRel,
            std::string& outError);

        /** Create a DataAsset (or derived) instance; AssetType stays "DataAsset". */
        std::shared_ptr<Asset> CreateDataAsset(
            const Reflection::MEClass* classInfo,
            const std::string& assetName,
            const std::string& directoryRel,
            std::string& outError);

        // CreateAsset<T> removed — use CreateAssetByTypeId.

        /** Helper for Saver Create implementations (unique Content-relative path). */
        std::string AllocateUniqueAssetRelativePath(
            std::string_view directoryRel,
            std::string_view baseName,
            std::string_view extension) const;

        bool SaveRegisteredAsset(const AssetMeta& meta, const Asset& asset) const;
        static AssetMeta MakePendingSaveMeta(
            const std::string& relativePath,
            const std::string& assetName,
            const std::string& assetType,
            const GUID& guid);
        void CacheCreatedAsset(const std::string& projectRelativePath, const std::shared_ptr<Asset>& asset);
        void NoteEditorFilesystemMutation(const std::filesystem::path& absolutePath) const;
        std::filesystem::path BuildMetaAbsolutePath(std::string_view projectRelativeAssetPath) const;

    private:
        template<typename T>
        std::shared_ptr<T> RemoveAsset(const std::string& path)
        {
            static_assert(AlwaysFalse<T>::value, "RemoveAsset<T> is not implemented for this type T");
            (void)path;
            return nullptr;
        }

        template<typename T>
        static std::shared_ptr<Asset> WrapLoadedAsset(
            std::shared_ptr<T> asset,
            std::string& outErrorMessage,
            const char* failureMessage)
        {
            if (asset == nullptr)
            {
                if (outErrorMessage.empty())
                {
                    outErrorMessage = failureMessage;
                }
                return nullptr;
            }

            return std::static_pointer_cast<Asset>(asset);
        }

        static std::shared_ptr<Asset> LoadHandler_StaticMesh(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_SkeletalMesh(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Skeleton(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_AnimationClip(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_AnimationGraph(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Texture2D(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Scene(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Prefab(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Material(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_MaterialInstance(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_Sprite(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_SpriteAtlas(
            AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage);
        static std::shared_ptr<Asset> LoadHandler_DataAsset(
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

        friend void RegisterAllAssetPipelines(AssetManager& assetManager);
        friend class AssetContentMutation;
        friend class SceneLoader;
        friend class PrefabLoader;
        friend class MaterialLoader;
        friend class MaterialInstanceLoader;
        friend class SpriteLoader;
        friend class SpriteAtlasLoader;
        friend class DataAssetLoader;
        friend class AnimationGraphLoader;
        friend class NativeCopyImporter;
        friend class Texture2DImporter;
        friend class MeshImporter;
        friend class AnimationClipImporter;
        friend class Engine;
        friend class Testing::TestAccess<AssetManager>;

        static void SetInstance(AssetManager* instance);
        static AssetManager* s_Instance;

        std::shared_ptr<Asset> LoadAssetByMeta_Internal(const AssetMeta& meta, std::string& outErrorMessage);

        struct AsyncDataAssetWorkerResult;
        struct AsyncDataAssetLoadRecord;

        AsyncLoadHandle AsyncLoadAssetByMeta_Internal(
            const AssetMeta& meta,
            AsyncLoadAssetCallback onComplete);
        AsyncLoadHandle EnqueueAsyncLoadFailure(
            AsyncLoadAssetCallback onComplete,
            std::string errorMessage);
        AsyncLoadHandle EnqueueAsyncLoadCacheHit(
            std::shared_ptr<Asset> asset,
            AsyncLoadAssetCallback onComplete);
        void TryAssembleInFlightDataAsset(const std::shared_ptr<AsyncDataAssetLoadRecord>& record);
        void FinishInFlightDataAsset(const std::shared_ptr<AsyncDataAssetLoadRecord>& record);
        std::shared_ptr<Asset> JoinInFlightDataAssetLoad(
            const std::shared_ptr<AsyncDataAssetLoadRecord>& record,
            std::string& outErrorMessage);
        std::shared_ptr<Asset> AssembleDataAssetFromJson(
            const AssetMeta& meta,
            const Json& root,
            std::string& outErrorMessage);

        /** True when AssetType can map to more than one runtime class (today: DataAsset). */
        static bool RequiresRuntimeClassNameForLoad(std::string_view assetTypeId);
        void SyncRuntimeClassNameAfterSave(const AssetMeta& meta, const Asset& asset) const;
        bool ValidateOrHealRuntimeClassNameAfterLoad(
            const AssetMeta& meta,
            const Asset& asset,
            std::string& outErrorMessage);

        std::string NormalizeProjectRelativeAssetPath(const std::string& path) const;
        bool IsUnderProjectContentRoot(const std::filesystem::path& absolutePath) const;
        bool IsUnderEngineDefaultAssetsRoot(const std::filesystem::path& absolutePath) const;

        void CacheMeta(const AssetMeta& meta, bool alreadyRegistered);
        void UncacheMeta(std::string_view projectRelativePath);

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
        void RemoveOrphanMetaFilesInDirectory(const std::filesystem::path& directory);

        AssetRegistry m_Registry;
        std::unordered_map<std::string, std::weak_ptr<MEObject>> m_LoadedAssetCache;
        std::unordered_map<std::string, AssetLoadHandlerFn> m_LoadHandlers;
        std::unordered_map<std::string, AssetSaveHandlerFn> m_SaveHandlers;
        std::unordered_map<std::string, AssetCreateHandlerFn> m_CreateHandlers;
        std::vector<ImportProductDescriptor> m_ImportProducts;
        int m_RegistryBroadcastBatchDepth = 0;

        std::unordered_map<std::string, std::shared_ptr<AsyncDataAssetLoadRecord>> m_InFlightDataAssetLoads;
    };

} // namespace minEngine

