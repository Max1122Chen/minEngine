#include "AssetManager.h"
#include "AssetContentMutation.h"

#include "AssetPipelineBootstrap.h"
#include "AssetTypeRegistry.h"
#include "Runtime/Resource/EditorFilesystemMutationPass.h"
#include "Runtime/Resource/Loaders/AssimpMeshImportUtil.h"
#include "Runtime/Resource/Loaders/AnimationClipLoader.h"
#include "Runtime/Resource/Loaders/AnimationGraphLoader.h"
#include "Runtime/Resource/Loaders/AudioClipLoader.h"
#include "Runtime/Resource/Loaders/EnvironmentMapLoader.h"
#include "Runtime/Resource/Loaders/FontLoader.h"
#include "Runtime/Resource/Loaders/LuaScriptLoader.h"
#include "Runtime/Resource/Loaders/MaterialInstanceLoader.h"
#include "Runtime/Resource/Loaders/MaterialLoader.h"
#include "Runtime/Resource/Loaders/PrefabLoader.h"
#include "Runtime/Resource/Loaders/SceneLoader.h"
#include "Runtime/Resource/Loaders/SkeletalMeshLoader.h"
#include "Runtime/Resource/Loaders/SkeletonLoader.h"
#include "Runtime/Resource/DataAsset.h"
#include "Runtime/Resource/Loaders/DataAssetLoader.h"
#include "Runtime/Resource/Loaders/SpriteLoader.h"
#include "Runtime/Resource/Loaders/StaticMeshLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/SkeletalMesh.h"
#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Function/Animation/Skeleton.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialInstance.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Resource/Font.h"
#include "Runtime/Resource/LuaScript.h"
#include "Runtime/Resource/AudioClip.h"
#include "Runtime/Function/Render/Environment/EnvironmentMap.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Core/Object/ObjectManager.h"

#include "AssetMeta.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace minEngine
{
    AssetManager* AssetManager::s_Instance = nullptr;

    bool AssetManager::IsPathInsideDirectory(
        const std::filesystem::path& absolutePath,
        const std::filesystem::path& rootDirectory)
    {
        if (rootDirectory.empty() || absolutePath.empty())
        {
            return false;
        }

        const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(rootDirectory);
        const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(absolutePath);

        auto mismatchPair =
            std::mismatch(canonicalRoot.begin(), canonicalRoot.end(), canonicalPath.begin());
        return mismatchPair.first == canonicalRoot.end();
    }

    void AssetManager::SetInstance(AssetManager* instance)
    {
        s_Instance = instance;
    }

    AssetManager& AssetManager::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "AssetManager is not initialized");
        return *s_Instance;
    }

    bool AssetManager::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void AssetManager::Initialize()
    {
        AssetTypeRegistry::Get().RegisterBuiltinTypes();
        RegisterAllAssetPipelines(*this);
    }

    AssetManager::AssetRegistryBroadcastBatchScope::AssetRegistryBroadcastBatchScope()
    {
        if (HasInstance())
        {
            AssetManager::Get().BeginRegistryBroadcastBatch();
            m_Active = true;
        }
    }

    AssetManager::AssetRegistryBroadcastBatchScope::~AssetRegistryBroadcastBatchScope()
    {
        if (m_Active && HasInstance())
        {
            AssetManager::Get().EndRegistryBroadcastBatch();
        }
    }

    void AssetManager::BeginRegistryBroadcastBatch()
    {
        ++m_RegistryBroadcastBatchDepth;
        if (m_RegistryBroadcastBatchDepth == 1)
        {
            m_Registry.BeginBatch();
        }
    }

    void AssetManager::EndRegistryBroadcastBatch()
    {
        ME_ASSERT(m_RegistryBroadcastBatchDepth > 0, "EndRegistryBroadcastBatch without matching begin");
        --m_RegistryBroadcastBatchDepth;
        if (m_RegistryBroadcastBatchDepth == 0)
        {
            m_Registry.EndBatch();
        }
    }

    void AssetManager::NoteEditorFilesystemMutation(const std::filesystem::path& absolutePath) const
    {
        EditorFilesystemMutationPass::NoteMutatedAbsolutePath(absolutePath);
    }

    void AssetManager::Shutdown()
    {
        m_RegistryBroadcastBatchDepth = 0;
        m_Registry.Shutdown();
        m_LoadedAssetCache.clear();
        m_LoadHandlers.clear();
        m_SaveHandlers.clear();
        m_CreateHandlers.clear();
        m_ImportProducts.clear();
        EditorFilesystemMutationPass::Clear();
    }

    void AssetManager::MarkReachableLoadedAssets(const std::function<void(MEObject*)>& markReachable) const
    {
        for (const auto& [path, weakAsset] : m_LoadedAssetCache)
        {
            (void)path;
            const std::shared_ptr<MEObject> loadedObject = weakAsset.lock();
            if (!loadedObject)
            {
                continue;
            }

            markReachable(loadedObject.get());

            Scene* sceneAsset = dynamic_cast<Scene*>(loadedObject.get());
            if (sceneAsset != nullptr)
            {
                sceneAsset->MarkReachableObjects(markReachable);
            }
        }
    }

    bool AssetManager::IsUnderProjectContentRoot(const std::filesystem::path& absolutePath) const
    {
        return IsPathInsideDirectory(absolutePath, PathRegistry::Get().GetProjectContentRoot());
    }

    bool AssetManager::IsUnderEngineDefaultAssetsRoot(const std::filesystem::path& absolutePath) const
    {
        return IsPathInsideDirectory(absolutePath, PathRegistry::Get().GetEngineDefaultAssetsRoot());
    }

    std::string AssetManager::NormalizeProjectRelativeAssetPath(const std::string& path) const
    {
        if (path.empty())
        {
            return std::string();
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        const std::filesystem::path input(path);

        std::filesystem::path absolutePath;
        if (input.is_absolute())
        {
            absolutePath = std::filesystem::weakly_canonical(input);
        }
        else if (!contentRoot.empty())
        {
            absolutePath = std::filesystem::weakly_canonical(contentRoot / input);
        }
        else
        {
            absolutePath = std::filesystem::weakly_canonical(std::filesystem::absolute(input));
        }

        if (IsUnderEngineDefaultAssetsRoot(absolutePath))
        {
            ME_LOG(LogAsset, Warn, 
                "Asset path is under EngineDefaultAssetsRoot and will not be registered: {}",
                absolutePath.string());
            return std::string();
        }

        if (!contentRoot.empty() && IsUnderProjectContentRoot(absolutePath))
        {
            std::error_code errorCode;
            const std::filesystem::path relativePath =
                std::filesystem::relative(absolutePath, std::filesystem::weakly_canonical(contentRoot), errorCode);
            if (!errorCode)
            {
                std::string result = relativePath.lexically_normal().generic_string();
                if (result.size() >= 2 && result[0] == '.' && result[1] == '.')
                {
                    return std::string();
                }

                return result;
            }
        }

        ME_LOG(LogAsset, Warn, "Asset path is not under ProjectContentRoot: {}", absolutePath.string());
        return std::string();
    }

    std::filesystem::path AssetManager::ResolveAssetAbsolutePath(std::string_view projectRelativeOrLegacyPath) const
    {
        return std::filesystem::path(ResolveAssetAbsolutePathString(projectRelativeOrLegacyPath));
    }

    std::string AssetManager::ResolveAssetAbsolutePathString(std::string_view projectRelativeOrLegacyPath) const
    {
        if (projectRelativeOrLegacyPath.empty())
        {
            return std::string();
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        const std::filesystem::path input(projectRelativeOrLegacyPath);

        if (input.is_absolute())
        {
            return std::filesystem::weakly_canonical(input).generic_string();
        }

        if (!contentRoot.empty())
        {
            return std::filesystem::weakly_canonical(contentRoot / input).generic_string();
        }

        return std::filesystem::weakly_canonical(std::filesystem::absolute(input)).generic_string();
    }

    std::filesystem::path AssetManager::BuildMetaAbsolutePath(std::string_view projectRelativeAssetPath) const
    {
        const std::string absoluteAssetPath = ResolveAssetAbsolutePathString(projectRelativeAssetPath);
        return std::filesystem::path(absoluteAssetPath + ".meta");
    }

    void AssetManager::CacheMeta(const AssetMeta& meta, bool alreadyRegistered)
    {
        m_Registry.CacheMeta(meta, alreadyRegistered);
    }

    void AssetManager::UncacheMeta(std::string_view projectRelativePath)
    {
        m_LoadedAssetCache.erase(std::string(projectRelativePath));
        m_Registry.UncacheMeta(projectRelativePath);
    }

    uint32_t AssetManager::Subscribe(AssetRegistryChangedCallback callback)
    {
        return m_Registry.Subscribe(std::move(callback));
    }

    void AssetManager::Unsubscribe(uint32_t subscriptionId)
    {
        m_Registry.Unsubscribe(subscriptionId);
    }

    void AssetManager::ScanAssets(const std::filesystem::path& directory)
    {
        if (!std::filesystem::exists(directory))
        {
            ME_LOG(LogAsset, Warn, "Skip scanning assets because directory does not exist: {}", directory.string());
            return;
        }

        if (!std::filesystem::is_directory(directory))
        {
            ME_LOG(LogAsset, Warn, "Skip scanning assets because path is not a directory: {}", directory.string());
            return;
        }

        RemoveOrphanMetaFilesInDirectory(directory);

        const AssetTypeRegistry& typeRegistry = AssetTypeRegistry::Get();

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path assetPath = entry.path().lexically_normal();

            // Assets/Sources/** holds Import Sources only — never Infer as engine mesh assets.
            bool underImportSources = false;
            for (const std::filesystem::path& part : assetPath)
            {
                if (part == "Sources")
                {
                    underImportSources = true;
                    break;
                }
            }
            if (underImportSources)
            {
                continue;
            }

            const std::string assetTypeId = typeRegistry.InferAssetTypeFromExtension(assetPath);
            if (!assetTypeId.empty())
            {
                AssetMeta meta = RegisterAsset(assetPath.string(), assetTypeId);
                if (assetTypeId == "Scene")
                {
                    SceneManager::Get().RegisterScene(meta.AssetName, meta.AssetPath);
                }
                continue;
            }

            // migration: re-register legacy Texture2D that still live as Content PNG/JPG
            // with an existing .meta (new imports are .metex only).
            std::string extension = assetPath.extension().string();
            for (char& ch : extension)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
            {
                const std::filesystem::path metaPath = BuildMetaAbsolutePath(
                    NormalizeProjectRelativeAssetPath(assetPath.string()));
                if (std::filesystem::exists(metaPath))
                {
                    RegisterAsset(assetPath.string(), "Texture2D");
                }
            }
        }
    }

    AssetMeta AssetManager::RegisterAsset(
        const std::string& path,
        const std::string& assetTypeId,
        const GUID* preferredGuid)
    {
        const std::string projectRelativePath = NormalizeProjectRelativeAssetPath(path);
        if (projectRelativePath.empty())
        {
            return AssetMeta();
        }

        const std::filesystem::path absoluteAssetPath = ResolveAssetAbsolutePath(projectRelativePath);
        std::error_code fileError;
        if (!std::filesystem::exists(absoluteAssetPath, fileError)
            || !std::filesystem::is_regular_file(absoluteAssetPath, fileError))
        {
            std::string metaError;
            RemoveMetaFileOnDisk(projectRelativePath, metaError);
            if (!metaError.empty())
            {
                ME_LOG(LogAsset, Warn, 
                    "RegisterAsset: asset file missing for '{}'; failed to remove stale meta: {}",
                    projectRelativePath,
                    metaError);
            }

            return AssetMeta();
        }

        const bool alreadyRegistered = m_Registry.ContainsPath(projectRelativePath);

        const std::filesystem::path metaPath = BuildMetaAbsolutePath(projectRelativePath);
        const std::string inferredAssetName = std::filesystem::path(projectRelativePath).stem().string();

        const Serialization::SerializerOptions metaSerializerOptions{
            .enumAsString = true,
            .strictTypeCheck = false,
            .skipUnknownField = true};

        auto loadMetaFromFile = [&](AssetMeta& outMeta) -> bool
        {
            Serialization::JsonReaderArchive archive;
            const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
                metaPath.string(),
                &outMeta,
                archive,
                metaSerializerOptions);

            if (!result.ok)
            {
                ME_LOG(LogAsset, Warn, "Failed to deserialize asset meta. Error: {}. Field path: {}. Meta file: {}",
                             result.message,
                             result.fieldPath,
                             metaPath.string());
                return false;
            }

            return true;
        };

        auto saveMetaToFile = [&](const AssetMeta& inMeta) -> bool
        {
            Serialization::JsonWriterArchive archive;
            const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
                metaPath.string(),
                &inMeta,
                archive,
                metaSerializerOptions);

            if (!result.ok)
            {
                ME_LOG(LogAsset, Warn, "Failed to serialize asset meta. Error: {}. Field path: {}. Meta file: {}",
                             result.message,
                             result.fieldPath,
                             metaPath.string());
                return false;
            }

            return true;
        };

        const bool hasPreferredGuid = preferredGuid != nullptr && !preferredGuid->IsZero();

        AssetMeta meta;
        bool loadedExistingMeta = false;
        if (std::filesystem::exists(metaPath))
        {
            loadedExistingMeta = loadMetaFromFile(meta);
            if (!loadedExistingMeta)
            {
                ME_LOG(LogAsset, Warn, "Failed to parse asset meta, regenerate it: {}", metaPath.string());
            }
        }

        if (!loadedExistingMeta)
        {
            meta.AssetName = inferredAssetName;
            meta.AssetPath = projectRelativePath;
            meta.AssetType = assetTypeId;
            meta.Guid = hasPreferredGuid ? *preferredGuid : GenerateGUID();

            if (!saveMetaToFile(meta))
            {
                ME_LOG(LogAsset, Warn, "Failed to save new asset meta: {}", metaPath.string());
            }
        }
        else
        {
            bool needsRewrite = false;
            if (meta.AssetName.empty())
            {
                meta.AssetName = inferredAssetName;
                needsRewrite = true;
            }

            const std::string normalizedStoredPath = NormalizeProjectRelativeAssetPath(meta.AssetPath);
            if (!normalizedStoredPath.empty() && meta.AssetPath != normalizedStoredPath)
            {
                meta.AssetPath = normalizedStoredPath;
                needsRewrite = true;
            }
            else if (meta.AssetPath != projectRelativePath)
            {
                meta.AssetPath = projectRelativePath;
                needsRewrite = true;
            }

            if (meta.AssetType.empty() || meta.AssetType != assetTypeId)
            {
                meta.AssetType = assetTypeId;
                needsRewrite = true;
            }

            if (meta.Guid.High == 0 && meta.Guid.Low == 0)
            {
                meta.Guid = hasPreferredGuid ? *preferredGuid : GenerateGUID();
                needsRewrite = true;
            }

            if (needsRewrite && !saveMetaToFile(meta))
            {
                ME_LOG(LogAsset, Warn, "Failed to rewrite asset meta: {}", metaPath.string());
            }
        }

        const AssetMeta* existingGuidMeta = m_Registry.FindMetaByGuid(meta.Guid);
        if (existingGuidMeta != nullptr && existingGuidMeta->AssetPath != projectRelativePath)
        {
            if (hasPreferredGuid && meta.Guid == *preferredGuid)
            {
                ME_LOG(LogAsset, Error,
                    "RegisterAsset: preferred Guid '{}' for '{}' collides with '{}'.",
                    preferredGuid->ToString(),
                    projectRelativePath,
                    existingGuidMeta->AssetPath);
                return AssetMeta();
            }

            ME_LOG(LogAsset, Warn, 
                "GUID collision detected between '{}' and '{}'. Regenerating GUID for current asset.",
                existingGuidMeta->AssetPath,
                projectRelativePath);
            meta.Guid = GenerateGUID();
            if (!saveMetaToFile(meta))
            {
                ME_LOG(LogAsset, Warn, "Failed to save regenerated GUID to asset meta: {}", metaPath.string());
            }
        }

        CacheMeta(meta, alreadyRegistered);

        ME_LOG(LogAsset, Info, "Asset {}: type='{}', path='{}', guid='{}'",
                     alreadyRegistered ? "updated" : "registered",
                     meta.AssetType,
                     meta.AssetPath,
                     meta.Guid.ToString());

        return meta;
    }

    void AssetManager::CacheCreatedAsset(
        const std::string& projectRelativePath,
        const std::shared_ptr<Asset>& asset)
    {
        if (!asset)
        {
            return;
        }

        const std::string registryKey = NormalizeProjectRelativeAssetPath(projectRelativePath);
        if (registryKey.empty())
        {
            return;
        }

        AssetMeta* meta = const_cast<AssetMeta*>(FindAssetMetaByPath(registryKey));
        if (meta != nullptr)
        {
            asset->SetMeta(meta);
            if (asset->GetGuid() != meta->Guid)
            {
                asset->SetGuid(meta->Guid);
            }
        }

        m_LoadedAssetCache[registryKey] = asset;
    }

    bool AssetManager::WriteOrUpdateMetaFile(const AssetMeta& meta)
    {
        return WriteMetaFile(meta);
    }

    void AssetManager::ApplyMetaIdentity(MEObject& object, const AssetMeta& meta)
    {
        if (!meta.AssetName.empty())
        {
            object.SetName(meta.AssetName);
        }
        object.SetGuid(meta.Guid);
    }

    void AssetManager::ClearProjectRegistry()
    {
        m_Registry.ClearRegistryData();
        m_LoadedAssetCache.clear();
    }

    void AssetManager::EvictLoadedAssetCache(std::string_view projectRelativePath)
    {
        m_LoadedAssetCache.erase(std::string(projectRelativePath));
    }

    void AssetManager::MoveLoadedAssetCacheKey(std::string_view oldRel, std::string_view newRel)
    {
        const std::string oldKey(oldRel);
        const std::string newKey(newRel);
        if (oldKey == newKey)
        {
            return;
        }

        auto cacheIter = m_LoadedAssetCache.find(oldKey);
        if (cacheIter == m_LoadedAssetCache.end())
        {
            return;
        }

        std::weak_ptr<MEObject> weakAsset = cacheIter->second;
        m_LoadedAssetCache.erase(cacheIter);
        m_LoadedAssetCache.emplace(newKey, std::move(weakAsset));
    }

    bool AssetManager::LogReferenceWarningsForDelete(const AssetMeta& meta) const
    {
        if (meta.AssetType == "Prefab")
        {
            return true;
        }

        ME_LOG(LogAsset, Warn, 
            "DeleteAsset: reference scan is not implemented (v0); proceeding with '{}'.",
            meta.AssetPath);
        return true;
    }

    bool AssetManager::WriteMetaFile(const AssetMeta& meta) const
    {
        const std::filesystem::path metaPath = BuildMetaAbsolutePath(meta.AssetPath);

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            metaPath.string(),
            &meta,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = true});

        if (!result.ok)
        {
            ME_LOG(LogAsset, Warn, 
                "Failed to serialize asset meta. Error: {}. Field path: {}. Meta file: {}",
                result.message,
                result.fieldPath,
                metaPath.string());
            return false;
        }

        return true;
    }

    bool AssetManager::MoveRegistryEntry(
        std::string_view oldRel,
        std::string_view newRel,
        AssetMeta& inOutMeta)
    {
        const std::string newKey(newRel);
        inOutMeta.AssetPath = newKey;
        inOutMeta.AssetName = std::filesystem::path(newKey).stem().string();
        return m_Registry.MoveMeta(oldRel, newRel);
    }

    bool AssetManager::DeleteAsset(const std::string& assetPath, std::string& outError)
    {
        return AssetContentMutation(*this).DeleteAsset(assetPath, outError, false);
    }

    bool AssetManager::DeleteAsset(
        const std::string& assetPath,
        std::string& outError,
        bool bUnpackOpenPrefabInstanceRefs)
    {
        return AssetContentMutation(*this).DeleteAsset(
            assetPath,
            outError,
            bUnpackOpenPrefabInstanceRefs);
    }

    bool AssetManager::UnregisterAsset(const std::string& assetPath, std::string& outError)
    {
        outError.clear();

        const std::string projectRelative = NormalizeProjectRelativeAssetPath(assetPath);
        if (projectRelative.empty())
        {
            outError = "invalid or out-of-project path";
            return false;
        }

        const AssetMeta* metaPtr = FindAssetMetaByPath(projectRelative);
        const bool wasRegistered = (metaPtr != nullptr);

        if (wasRegistered)
        {
            const AssetMeta meta = *metaPtr;

            EvictLoadedAssetCache(projectRelative);
            UncacheMeta(projectRelative);

            if (meta.AssetType == "Scene" && SceneManager::HasInstance())
            {
                SceneManager::Get().UnregisterScene(meta.AssetName);
            }
        }

        std::string metaError;
        if (!RemoveMetaFileOnDisk(projectRelative, metaError))
        {
            outError = metaError;
            return false;
        }

        if (!wasRegistered)
        {
            outError = "asset not registered";
            return false;
        }

        return true;
    }

    bool AssetManager::RemoveMetaFileOnDisk(const std::string& assetPath, std::string& outError)
    {
        outError.clear();

        const std::string projectRelative = NormalizeProjectRelativeAssetPath(assetPath);
        if (projectRelative.empty())
        {
            outError = "invalid or out-of-project path";
            return false;
        }

        const std::filesystem::path metaAbsolutePath = BuildMetaAbsolutePath(projectRelative);
        std::error_code errorCode;
        if (!std::filesystem::exists(metaAbsolutePath, errorCode))
        {
            return true;
        }

        if (!std::filesystem::remove(metaAbsolutePath, errorCode) || errorCode)
        {
            outError = "failed to remove meta file: " + errorCode.message();
            return false;
        }

        return true;
    }

    void AssetManager::RemoveOrphanMetaFilesInDirectory(const std::filesystem::path& directory)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path metaPath = entry.path().lexically_normal();
            if (metaPath.extension() != ".meta")
            {
                continue;
            }

            const std::filesystem::path assetPath =
                metaPath.parent_path() / metaPath.stem();
            std::error_code errorCode;
            if (std::filesystem::exists(assetPath, errorCode)
                && std::filesystem::is_regular_file(assetPath, errorCode))
            {
                continue;
            }

            if (!std::filesystem::remove(metaPath, errorCode) || errorCode)
            {
                ME_LOG(LogAsset, Warn, 
                    "RemoveOrphanMetaFilesInDirectory: failed to remove orphan meta '{}': {}",
                    metaPath.string(),
                    errorCode.message());
                continue;
            }

            ME_LOG(LogAsset, Info, 
                "RemoveOrphanMetaFilesInDirectory: removed orphan meta '{}'",
                metaPath.string());
        }
    }

    bool AssetManager::MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError)
    {
        return AssetContentMutation(*this).MoveAsset(oldPath, newPath, outError);
    }

    bool AssetManager::RenameAsset(const std::string& oldPath, const std::string& newFileName, std::string& outError)
    {
        return AssetContentMutation(*this).RenameAsset(oldPath, newFileName, outError);
    }

    bool AssetManager::CreateFolder(
        const std::string& parentDirectoryRel,
        const std::string& folderName,
        std::string& outError,
        std::string* outCreatedDirectoryRel)
    {
        return AssetContentMutation(*this).CreateFolder(
            parentDirectoryRel,
            folderName,
            outError,
            outCreatedDirectoryRel);
    }

    bool AssetManager::RenameFolder(
        const std::string& directoryRel,
        const std::string& newFolderName,
        std::string& outError)
    {
        return AssetContentMutation(*this).RenameFolder(directoryRel, newFolderName, outError);
    }

    bool AssetManager::DeleteFolder(
        const std::string& directoryRel,
        std::string& outError,
        bool requireEmpty)
    {
        return AssetContentMutation(*this).DeleteFolder(directoryRel, outError, requireEmpty);
    }


    std::shared_ptr<Asset> AssetManager::LoadAssetByPath(const std::string& path, std::string& outErrorMessage)
    {
        outErrorMessage.clear();

        const AssetMeta* meta = FindAssetMetaByPath(path);
        if (meta == nullptr)
        {
            outErrorMessage = "asset meta not found for path: " + path;
            return nullptr;
        }
        return LoadAssetByMeta_Internal(*meta, outErrorMessage);
    }

    std::shared_ptr<Asset> AssetManager::LoadAssetByMeta(const AssetMeta& meta, std::string& outErrorMessage)
    {
        outErrorMessage.clear();
        return LoadAssetByMeta_Internal(meta, outErrorMessage);
    }

    std::shared_ptr<Asset> AssetManager::LoadAssetByGUID(const GUID& guid, std::string& outErrorMessage)
    {
        outErrorMessage.clear();

        const AssetMeta* assetMeta = FindAssetMetaByGuid(guid);
        if (assetMeta == nullptr)
        {
            outErrorMessage = "guid not found in object manager or asset registry";
            return nullptr;
        }

        return LoadAssetByMeta_Internal(*assetMeta, outErrorMessage);
    }

    const AssetMeta* AssetManager::FindAssetMetaByPath(const std::string& path) const
    {
        const std::string registryKey = NormalizeProjectRelativeAssetPath(path);
        if (!registryKey.empty())
        {
            return m_Registry.FindMetaByPath(registryKey);
        }

        return m_Registry.FindMetaByPath(path);
    }

    const AssetMeta* AssetManager::FindAssetMetaByGuid(const GUID& guid) const
    {
        return m_Registry.FindMetaByGuid(guid);
    }

    std::vector<const AssetMeta*> AssetManager::FindAssetMetasByType(const std::string& assetTypeId) const
    {
        return m_Registry.FindMetasByType(assetTypeId);
    }

    std::vector<const AssetMeta*> AssetManager::FindAssetMetasByClass(const Reflection::MEClass* assetClass) const
    {
        const std::vector<std::string> typeIds =
            AssetTypeRegistry::Get().CollectAssetTypeIdsAssignableTo(assetClass);
        std::vector<const AssetMeta*> result;
        for (const std::string& typeId : typeIds)
        {
            const std::vector<const AssetMeta*> metas = FindAssetMetasByType(typeId);
            result.insert(result.end(), metas.begin(), metas.end());
        }

        return result;
    }

    std::vector<const AssetMeta*> AssetManager::FindAssetMetasByRuntimeClass(
        const std::string& runtimeClassName) const
    {
        const std::string assetTypeId =
            AssetTypeRegistry::Get().InferAssetTypeFromRuntimeClassName(runtimeClassName);
        if (assetTypeId.empty())
        {
            return {};
        }

        return FindAssetMetasByType(assetTypeId);
    }

    std::vector<const AssetMeta*> AssetManager::FindAssetMetasUnderDirectory(
        std::string_view projectRelativeDirectory) const
    {
        return m_Registry.FindMetasUnderDirectory(projectRelativeDirectory);
    }

    void AssetManager::RegisterLoadHandler(std::string_view assetTypeId, AssetLoadHandlerFn handler)
    {
        if (assetTypeId.empty() || handler == nullptr)
        {
            ME_LOG(LogAsset, Error, "RegisterLoadHandler: assetTypeId and handler are required");
            return;
        }

        m_LoadHandlers[std::string(assetTypeId)] = handler;
    }

    AssetLoadHandlerFn AssetManager::FindLoadHandler(std::string_view assetTypeId) const
    {
        const auto it = m_LoadHandlers.find(std::string(assetTypeId));
        if (it == m_LoadHandlers.end())
        {
            return nullptr;
        }

        return it->second;
    }

    void AssetManager::RegisterSaveHandler(std::string_view assetTypeId, AssetSaveHandlerFn handler)
    {
        if (assetTypeId.empty() || handler == nullptr)
        {
            ME_LOG(LogAsset, Error, "RegisterSaveHandler: assetTypeId and handler are required");
            return;
        }

        m_SaveHandlers[std::string(assetTypeId)] = handler;
    }

    AssetSaveHandlerFn AssetManager::FindSaveHandler(std::string_view assetTypeId) const
    {
        const auto it = m_SaveHandlers.find(std::string(assetTypeId));
        if (it == m_SaveHandlers.end())
        {
            return nullptr;
        }

        return it->second;
    }

    bool AssetManager::SaveRegisteredAsset(const AssetMeta& meta, const Asset& asset) const
    {
        const AssetSaveHandlerFn handler = FindSaveHandler(meta.AssetType);
        if (handler == nullptr)
        {
            ME_LOG(LogAsset, Error, "SaveAsset: no Saver registered for type '{}'", meta.AssetType);
            return false;
        }

        std::string error;
        if (!handler(*this, meta, asset, error))
        {
            if (!error.empty())
            {
                ME_LOG(
                    LogAsset,
                    Error,
                    "SaveAsset: '{}' ({}) failed: {}",
                    meta.AssetPath,
                    meta.AssetType,
                    error);
            }
            return false;
        }

        return true;
    }

    AssetMeta AssetManager::MakePendingSaveMeta(
        const std::string& relativePath,
        const std::string& assetName,
        const std::string& assetType,
        const GUID& guid)
    {
        AssetMeta meta;
        meta.AssetPath = relativePath;
        meta.AssetName = assetName;
        meta.AssetType = assetType;
        meta.Guid = guid;
        return meta;
    }

    void AssetManager::RegisterImportProduct(const ImportProductDescriptor& descriptor)
    {
        if (descriptor.ProductId.empty() || descriptor.Import == nullptr
            || descriptor.AcceptsSourceExtension == nullptr)
        {
            ME_LOG(LogAsset, Error, 
                "RegisterImportProduct: ProductId, AcceptsSourceExtension, and Import are required");
            return;
        }

        for (ImportProductDescriptor& existing : m_ImportProducts)
        {
            if (existing.ProductId == descriptor.ProductId)
            {
                existing = descriptor;
                return;
            }
        }

        m_ImportProducts.push_back(descriptor);
    }

    const ImportProductDescriptor* AssetManager::FindImportProduct(std::string_view productId) const
    {
        for (const ImportProductDescriptor& descriptor : m_ImportProducts)
        {
            if (descriptor.ProductId == productId)
            {
                return &descriptor;
            }
        }

        return nullptr;
    }

    ImportResult AssetManager::Import(const ImportRequest& request)
    {
        ImportResult result;
        if (request.ProductId.empty())
        {
            result.ErrorMessage = "ImportRequest.ProductId is empty";
            return result;
        }

        const ImportProductDescriptor* product = FindImportProduct(request.ProductId);
        if (product == nullptr)
        {
            result.ErrorMessage = "unknown import product '" + request.ProductId + "'";
            return result;
        }

        const std::string extension = request.SourcePath.extension().string();
        if (product->AcceptsSourceExtension != nullptr
            && !product->AcceptsSourceExtension(extension))
        {
            result.ErrorMessage =
                "import product '" + request.ProductId + "' does not accept extension '"
                + extension + "'";
            return result;
        }

        return product->Import(*this, request);
    }

    bool AssetManager::Reimport(const std::string& assetPath, std::string& outError)
    {
        const std::string registryKey = NormalizeProjectRelativeAssetPath(assetPath);
        const AssetMeta* meta = FindAssetMetaByPath(registryKey);
        if (meta == nullptr)
        {
            outError = "asset not found: " + assetPath;
            return false;
        }

        if (meta->SourcePath.empty())
        {
            outError = "asset has no SourcePath (cannot reimport): " + registryKey;
            return false;
        }

        std::string productId;
        if (meta->AssetType == "StaticMesh")
        {
            productId = "StaticMesh";
        }
        else if (meta->AssetType == "SkeletalMesh")
        {
            productId = "SkeletalMesh";
        }
        else if (meta->AssetType == "AnimationClip")
        {
            productId = "AnimationClip";
        }
        else if (meta->AssetType == "Texture2D")
        {
            productId = "Texture2D";
        }
        else
        {
            outError = "reimport is not supported for asset type '" + meta->AssetType + "'";
            return false;
        }

        const std::filesystem::path sourceAbsolute = ResolveAssetAbsolutePath(meta->SourcePath);
        if (!std::filesystem::exists(sourceAbsolute) || !std::filesystem::is_regular_file(sourceAbsolute))
        {
            outError = "SourcePath is missing on disk: " + meta->SourcePath;
            return false;
        }

        const std::filesystem::path assetAbsolute = ResolveAssetAbsolutePath(meta->AssetPath);
        ImportRequest request;
        request.SourcePath = sourceAbsolute;
        request.DestDirectory = assetAbsolute.parent_path();
        request.ProductId = productId;
        request.bOverwriteExisting = true;

        if (productId == "AnimationClip")
        {
            std::string loadError;
            std::shared_ptr<Asset> loaded = LoadAssetByPath(meta->AssetPath, loadError);
            std::shared_ptr<AnimationClip> clip = std::dynamic_pointer_cast<AnimationClip>(loaded);
            if (clip != nullptr && clip->GetSkeleton() != nullptr)
            {
                const Skeleton* skeleton = clip->GetSkeleton();
                if (skeleton->GetMeta() != nullptr && !skeleton->GetMeta()->AssetPath.empty())
                {
                    request.SkeletonAssetPath = skeleton->GetMeta()->AssetPath;
                }
                else
                {
                    const AssetMeta* skeletonMeta = FindAssetMetaByGuid(skeleton->GetGuid());
                    if (skeletonMeta != nullptr)
                    {
                        request.SkeletonAssetPath = skeletonMeta->AssetPath;
                    }
                }
            }
        }

        const ImportResult result = Import(request);
        if (!result.bSuccess)
        {
            outError = result.ErrorMessage.empty() ? "reimport failed" : result.ErrorMessage;
            return false;
        }

        return true;
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_StaticMesh(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            StaticMeshLoader::LoadFromAssetMeta(meta),
            outErrorMessage,
            "failed to load StaticMesh");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_SkeletalMesh(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            SkeletalMeshLoader::LoadFromAssetMeta(meta),
            outErrorMessage,
            "failed to load SkeletalMesh");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Skeleton(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(SkeletonLoader::Load(meta), outErrorMessage, "failed to load Skeleton");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_AnimationClip(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            AnimationClipLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load AnimationClip");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_AnimationGraph(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            AnimationGraphLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load AnimationGraph");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Texture2D(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            Texture2DLoader::LoadFromAssetMeta(meta),
            outErrorMessage,
            "failed to load Texture2D");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Scene(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(SceneLoader::Load(meta), outErrorMessage, "failed to load Scene");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Prefab(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            PrefabLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load Prefab");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Material(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            MaterialLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load Material");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_MaterialInstance(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            MaterialInstanceLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load MaterialInstance");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Sprite(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            SpriteLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load Sprite");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_SpriteAtlas(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            SpriteAtlasLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load SpriteAtlas");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_DataAsset(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            DataAssetLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load DataAsset");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Font(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            FontLoader::LoadFromAssetMeta(meta),
            outErrorMessage,
            "failed to load Font");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_LuaScript(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            LuaScriptLoader::LoadFromAssetMeta(meta),
            outErrorMessage,
            "failed to load LuaScript");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_EnvironmentMap(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            EnvironmentMapLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load EnvironmentMap");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_AudioClip(
        AssetManager& /*manager*/, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return WrapLoadedAsset(
            AudioClipLoader::Load(meta, &outErrorMessage),
            outErrorMessage,
            "failed to load AudioClip");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_ShaderRemoved(
        AssetManager& /*manager*/, const AssetMeta& /*meta*/, std::string& outErrorMessage)
    {
        outErrorMessage = "Shader assets are removed; use Material compile instead.";
        return nullptr;
    }

    std::shared_ptr<Asset> AssetManager::LoadAssetByMeta_Internal(const AssetMeta& meta, std::string& outErrorMessage)
    {
        const std::string registryKey = NormalizeProjectRelativeAssetPath(meta.AssetPath);
        if (!registryKey.empty())
        {
            const auto cacheIt = m_LoadedAssetCache.find(registryKey);
            if (cacheIt != m_LoadedAssetCache.end())
            {
                std::shared_ptr<MEObject> cachedObject = cacheIt->second.lock();
                if (cachedObject)
                {
                    return std::dynamic_pointer_cast<Asset>(cachedObject);
                }
            }
        }

        const AssetLoadHandlerFn handler = FindLoadHandler(meta.AssetType);
        if (handler == nullptr)
        {
            outErrorMessage = "unsupported asset type '" + meta.AssetType + "'";
            return nullptr;
        }

        std::shared_ptr<Asset> asset = handler(*this, meta, outErrorMessage);
        if (asset == nullptr)
        {
            return nullptr;
        }

        if (!registryKey.empty())
        {
            m_LoadedAssetCache[registryKey] = asset;
            const AssetMeta* registryMeta = FindAssetMetaByPath(registryKey);
            if (registryMeta != nullptr)
            {
                asset->SetMeta(const_cast<AssetMeta*>(registryMeta));
            }
        }

        return asset;
    }


    std::string AssetManager::AllocateUniqueAssetRelativePath(
        std::string_view directoryRel,
        std::string_view baseName,
        std::string_view extension) const
    {
        std::string directory = std::string(directoryRel);
        if (directory.empty())
        {
            directory = "Assets";
        }

        std::string sanitizedBase = std::string(baseName);
        if (sanitizedBase.empty())
        {
            sanitizedBase = "NewAsset";
        }
        for (char& character : sanitizedBase)
        {
            const bool allowed = std::isalnum(static_cast<unsigned char>(character)) != 0
                || character == '_' || character == '-';
            if (!allowed)
            {
                character = '_';
            }
        }

        const std::filesystem::path directoryPath(directory);
        for (int suffixIndex = 0; suffixIndex < 1000; ++suffixIndex)
        {
            const std::string candidateBase = suffixIndex == 0
                ? sanitizedBase
                : sanitizedBase + "_" + std::to_string(suffixIndex);
            const std::filesystem::path relativePath =
                (directoryPath / (candidateBase + std::string(extension))).lexically_normal();
            const std::string normalizedPath = relativePath.generic_string();

            if (FindAssetMetaByPath(normalizedPath) != nullptr)
            {
                continue;
            }

            const std::filesystem::path absolutePath = ResolveAssetAbsolutePath(normalizedPath);
            std::error_code fileError;
            if (!std::filesystem::exists(absolutePath, fileError))
            {
                return normalizedPath;
            }
        }

        return std::string();
    }

    void AssetManager::RegisterCreateHandler(std::string_view assetTypeId, AssetCreateHandlerFn handler)
    {
        if (assetTypeId.empty() || handler == nullptr)
        {
            ME_LOG(LogAsset, Error, "RegisterCreateHandler: assetTypeId and handler are required");
            return;
        }

        m_CreateHandlers[std::string(assetTypeId)] = handler;
    }

    AssetCreateHandlerFn AssetManager::FindCreateHandler(std::string_view assetTypeId) const
    {
        const auto it = m_CreateHandlers.find(std::string(assetTypeId));
        if (it == m_CreateHandlers.end())
        {
            return nullptr;
        }

        return it->second;
    }

    std::shared_ptr<Asset> AssetManager::CreateAssetByTypeId(
        std::string_view assetTypeId,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        outError.clear();
        if (assetTypeId.empty())
        {
            outError = "assetTypeId is empty";
            return nullptr;
        }

        if (FindSaveHandler(assetTypeId) == nullptr)
        {
            outError = "no Saver registered for type '" + std::string(assetTypeId) + "'";
            ME_LOG(LogAsset, Error, "CreateAsset: {}", outError);
            return nullptr;
        }

        const AssetCreateHandlerFn create = FindCreateHandler(assetTypeId);
        if (create == nullptr)
        {
            outError = "no Create handler registered for type '" + std::string(assetTypeId) + "'";
            ME_LOG(LogAsset, Error, "CreateAsset: {}", outError);
            return nullptr;
        }

        return create(*this, assetName, directoryRel, outError);
    }


}
