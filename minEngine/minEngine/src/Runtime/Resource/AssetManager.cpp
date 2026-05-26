#include "AssetManager.h"

#include "AssetTypeRegistry.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Shader.h"
#include "Runtime/Resource/AssetResources/ShaderResource.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Core/Object/ObjectManager.h"

#include "AssetMeta.h"

#include <algorithm>

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
    }

    AssetManager::SuppressExternalSyncScope::SuppressExternalSyncScope()
    {
        if (HasInstance())
        {
            AssetManager::Get().BeginSuppressExternalSync();
            m_Active = true;
        }
    }

    AssetManager::SuppressExternalSyncScope::~SuppressExternalSyncScope()
    {
        if (m_Active && HasInstance())
        {
            AssetManager::Get().EndSuppressExternalSync();
        }
    }

    void AssetManager::BeginSuppressExternalSync()
    {
        ++m_SuppressExternalSyncCount;
    }

    void AssetManager::EndSuppressExternalSync()
    {
        ME_ASSERT(m_SuppressExternalSyncCount > 0, "EndSuppressExternalSync without matching begin");
        --m_SuppressExternalSyncCount;
    }

    bool AssetManager::IsExternalSyncSuppressed() const
    {
        return m_SuppressExternalSyncCount > 0;
    }

    void AssetManager::Shutdown()
    {
        m_SuppressExternalSyncCount = 0;
        m_Subscribers.clear();
        m_AssetMetasByType.clear();
        m_AssetPathByGuid.clear();
        m_AssetRegistry.clear();
        m_LoadedAssetCache.clear();
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
            ME_CORE_WARN(
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

        ME_CORE_WARN("Asset path is not under ProjectContentRoot: {}", absolutePath.string());
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

    void AssetManager::RemoveFromTypeBucket(const AssetMeta& meta)
    {
        auto bucketIter = m_AssetMetasByType.find(meta.AssetType);
        if (bucketIter == m_AssetMetasByType.end())
        {
            return;
        }

        std::vector<AssetMeta*>& bucket = bucketIter->second;
        auto registryIter = m_AssetRegistry.find(meta.AssetPath);
        if (registryIter == m_AssetRegistry.end())
        {
            return;
        }

        AssetMeta* target = &registryIter->second;
        bucket.erase(
            std::remove(bucket.begin(), bucket.end(), target),
            bucket.end());

        if (bucket.empty())
        {
            m_AssetMetasByType.erase(bucketIter);
        }
    }

    void AssetManager::AddToTypeBucket(const AssetMeta& meta)
    {
        auto registryIter = m_AssetRegistry.find(meta.AssetPath);
        if (registryIter == m_AssetRegistry.end())
        {
            return;
        }

        m_AssetMetasByType[meta.AssetType].push_back(&registryIter->second);
    }

    void AssetManager::CacheMeta(const AssetMeta& meta, bool alreadyRegistered)
    {
        auto existingIter = m_AssetRegistry.find(meta.AssetPath);
        if (existingIter != m_AssetRegistry.end())
        {
            RemoveFromTypeBucket(existingIter->second);
        }

        m_AssetRegistry[meta.AssetPath] = meta;
        m_AssetPathByGuid[meta.Guid] = meta.AssetPath;
        AddToTypeBucket(meta);

        AssetRegistryChange change;
        change.Kind = alreadyRegistered ? AssetRegistryChangeKind::MetaUpdated
                                        : AssetRegistryChangeKind::Registered;
        change.Guid = meta.Guid;
        change.NewPath = meta.AssetPath;
        change.AssetTypeId = meta.AssetType;
        BroadcastChange(change);
    }

    void AssetManager::UncacheMeta(std::string_view projectRelativePath)
    {
        const std::string key(projectRelativePath);
        auto registryIter = m_AssetRegistry.find(key);
        if (registryIter == m_AssetRegistry.end())
        {
            return;
        }

        const AssetMeta meta = registryIter->second;
        RemoveFromTypeBucket(meta);
        m_AssetPathByGuid.erase(meta.Guid);
        m_AssetRegistry.erase(registryIter);
        m_LoadedAssetCache.erase(key);

        AssetRegistryChange change;
        change.Kind = AssetRegistryChangeKind::Unregistered;
        change.Guid = meta.Guid;
        change.OldPath = meta.AssetPath;
        change.AssetTypeId = meta.AssetType;
        BroadcastChange(change);
    }

    void AssetManager::BroadcastChange(const AssetRegistryChange& change)
    {
        for (const auto& [subscriptionId, callback] : m_Subscribers)
        {
            (void)subscriptionId;
            if (callback)
            {
                callback(change);
            }
        }
    }

    uint32_t AssetManager::Subscribe(AssetRegistryChangedCallback callback)
    {
        if (!callback)
        {
            return kInvalidAssetRegistrySubscriptionId;
        }

        const uint32_t subscriptionId = m_NextSubscriptionId++;
        m_Subscribers[subscriptionId] = std::move(callback);
        return subscriptionId;
    }

    void AssetManager::Unsubscribe(uint32_t subscriptionId)
    {
        if (subscriptionId == kInvalidAssetRegistrySubscriptionId)
        {
            return;
        }

        m_Subscribers.erase(subscriptionId);
    }

    void AssetManager::ScanAssets(const std::filesystem::path& directory)
    {
        if (!std::filesystem::exists(directory))
        {
            ME_CORE_WARN("Skip scanning assets because directory does not exist: {}", directory.string());
            return;
        }

        if (!std::filesystem::is_directory(directory))
        {
            ME_CORE_WARN("Skip scanning assets because path is not a directory: {}", directory.string());
            return;
        }

        const AssetTypeRegistry& typeRegistry = AssetTypeRegistry::Get();

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path assetPath = entry.path().lexically_normal();
            const std::string assetTypeId = typeRegistry.InferAssetTypeFromExtension(assetPath);
            if (assetTypeId.empty())
            {
                continue;
            }

            AssetMeta meta = RegisterAsset(assetPath.string(), assetTypeId);

            if (assetTypeId == "Scene")
            {
                SceneManager::Get().RegisterScene(meta.AssetName, meta.AssetPath);
            }
        }
    }

    AssetMeta AssetManager::RegisterAsset(const std::string& path, const std::string& assetTypeId)
    {
        const std::string projectRelativePath = NormalizeProjectRelativeAssetPath(path);
        if (projectRelativePath.empty())
        {
            return AssetMeta();
        }

        const bool alreadyRegistered = (m_AssetRegistry.find(projectRelativePath) != m_AssetRegistry.end());

        const std::filesystem::path metaPath = BuildMetaAbsolutePath(projectRelativePath);
        const std::string inferredAssetName = std::filesystem::path(projectRelativePath).stem().string();

        const Serialization::SerializerOptions metaSerializerOptions{
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = true,
            .allowObjectPtrSerialization = false};

        auto loadMetaFromFile = [&](AssetMeta& outMeta) -> bool
        {
            Serialization::JsonReaderArchive archive;
            const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
                metaPath.string(),
                minEngine::Reflection::GetClassName<AssetMeta>(),
                &outMeta,
                archive,
                metaSerializerOptions);

            if (!result.ok)
            {
                ME_CORE_WARN("Failed to deserialize asset meta. Error: {}. Field path: {}. Meta file: {}",
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
                minEngine::Reflection::GetClassName<AssetMeta>(),
                &inMeta,
                archive,
                metaSerializerOptions);

            if (!result.ok)
            {
                ME_CORE_WARN("Failed to serialize asset meta. Error: {}. Field path: {}. Meta file: {}",
                             result.message,
                             result.fieldPath,
                             metaPath.string());
                return false;
            }

            return true;
        };

        AssetMeta meta;
        bool loadedExistingMeta = false;
        if (std::filesystem::exists(metaPath))
        {
            loadedExistingMeta = loadMetaFromFile(meta);
            if (!loadedExistingMeta)
            {
                ME_CORE_WARN("Failed to parse asset meta, regenerate it: {}", metaPath.string());
            }
        }

        if (!loadedExistingMeta)
        {
            meta.AssetName = inferredAssetName;
            meta.AssetPath = projectRelativePath;
            meta.AssetType = assetTypeId;
            meta.Guid = GenerateGUID();

            if (!saveMetaToFile(meta))
            {
                ME_CORE_WARN("Failed to save new asset meta: {}", metaPath.string());
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
                meta.Guid = GenerateGUID();
                needsRewrite = true;
            }

            if (needsRewrite && !saveMetaToFile(meta))
            {
                ME_CORE_WARN("Failed to rewrite asset meta: {}", metaPath.string());
            }
        }

        auto guidIt = m_AssetPathByGuid.find(meta.Guid);
        if (guidIt != m_AssetPathByGuid.end() && guidIt->second != projectRelativePath)
        {
            ME_CORE_WARN(
                "GUID collision detected between '{}' and '{}'. Regenerating GUID for current asset.",
                guidIt->second,
                projectRelativePath);
            meta.Guid = GenerateGUID();
            if (!saveMetaToFile(meta))
            {
                ME_CORE_WARN("Failed to save regenerated GUID to asset meta: {}", metaPath.string());
            }
        }

        CacheMeta(meta, alreadyRegistered);

        ME_CORE_INFO("Asset {}: type='{}', path='{}', guid='{}'",
                     alreadyRegistered ? "updated" : "registered",
                     meta.AssetType,
                     meta.AssetPath,
                     meta.Guid.ToString());

        return meta;
    }

    ImportAssetResult AssetManager::ImportAsset(const std::filesystem::path& sourcePath,
                                                const std::filesystem::path& destDirectory)
    {
        SuppressExternalSyncScope suppressScope;
        ImportAssetResult result;

        if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
        {
            result.ErrorMessage = "source file does not exist: " + sourcePath.string();
            return result;
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        if (contentRoot.empty())
        {
            result.ErrorMessage = "project content root is not set";
            return result;
        }

        std::filesystem::path absoluteDestDirectory = destDirectory.is_absolute()
            ? std::filesystem::weakly_canonical(destDirectory)
            : std::filesystem::weakly_canonical(contentRoot / destDirectory);

        if (!IsUnderProjectContentRoot(absoluteDestDirectory))
        {
            result.ErrorMessage = "destination directory is outside project Assets: " + absoluteDestDirectory.string();
            return result;
        }

        const AssetTypeRegistry& typeRegistry = AssetTypeRegistry::Get();
        const std::string assetTypeId = typeRegistry.InferAssetTypeFromExtension(sourcePath);
        if (assetTypeId.empty())
        {
            result.ErrorMessage = "unsupported file extension: " + sourcePath.extension().string();
            return result;
        }

        const std::filesystem::path destFilePath = absoluteDestDirectory / sourcePath.filename();
        if (std::filesystem::exists(destFilePath))
        {
            result.ErrorMessage = "destination file already exists: " + destFilePath.string();
            return result;
        }

        std::error_code copyError;
        std::filesystem::copy_file(sourcePath, destFilePath, std::filesystem::copy_options::none, copyError);
        if (copyError)
        {
            result.ErrorMessage = "copy failed: " + copyError.message();
            return result;
        }

        result.Meta = RegisterAsset(destFilePath.string(), assetTypeId);
        if (result.Meta.AssetPath.empty())
        {
            result.ErrorMessage = "failed to register imported asset";
            return result;
        }

        result.bSuccess = true;
        return result;
    }

    void AssetManager::ClearProjectRegistry()
    {
        m_AssetRegistry.clear();
        m_AssetPathByGuid.clear();
        m_AssetMetasByType.clear();
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
        ME_CORE_WARN(
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
            minEngine::Reflection::GetClassName<AssetMeta>(),
            &meta,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = true,
                .allowObjectPtrSerialization = false});

        if (!result.ok)
        {
            ME_CORE_WARN(
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
        const std::string oldKey(oldRel);
        const std::string newKey(newRel);

        auto oldIter = m_AssetRegistry.find(oldKey);
        if (oldIter == m_AssetRegistry.end())
        {
            return false;
        }

        AssetMeta meta = oldIter->second;
        RemoveFromTypeBucket(meta);
        m_AssetRegistry.erase(oldIter);

        inOutMeta.AssetPath = newKey;
        inOutMeta.AssetName = std::filesystem::path(newKey).stem().string();
        meta = inOutMeta;

        m_AssetRegistry.emplace(newKey, meta);
        m_AssetPathByGuid[meta.Guid] = newKey;
        AddToTypeBucket(meta);
        return true;
    }

    bool AssetManager::DeleteAsset(const std::string& assetPath, std::string& outError)
    {
        SuppressExternalSyncScope suppressScope;
        outError.clear();

        const std::string projectRelative = NormalizeProjectRelativeAssetPath(assetPath);
        if (projectRelative.empty())
        {
            outError = "invalid or out-of-project path";
            return false;
        }

        const AssetMeta* metaPtr = FindAssetMetaByPath(projectRelative);
        if (metaPtr == nullptr)
        {
            outError = "asset not registered";
            return false;
        }

        const AssetMeta meta = *metaPtr;
        LogReferenceWarningsForDelete(meta);

        const std::filesystem::path absolutePath = ResolveAssetAbsolutePath(projectRelative);
        const std::filesystem::path metaAbsolutePath = BuildMetaAbsolutePath(projectRelative);

        std::error_code removeError;
        if (std::filesystem::exists(absolutePath))
        {
            removeError.clear();
            if (!std::filesystem::remove(absolutePath, removeError) || removeError)
            {
                outError = "failed to remove asset file: " + removeError.message();
                return false;
            }
        }

        if (std::filesystem::exists(metaAbsolutePath))
        {
            removeError.clear();
            if (!std::filesystem::remove(metaAbsolutePath, removeError) || removeError)
            {
                outError = "failed to remove meta file: " + removeError.message();
                return false;
            }
        }

        EvictLoadedAssetCache(projectRelative);
        UncacheMeta(projectRelative);

        if (meta.AssetType == "Scene" && SceneManager::HasInstance())
        {
            SceneManager::Get().UnregisterScene(meta.AssetName);
        }

        return true;
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
        if (metaPtr == nullptr)
        {
            outError = "asset not registered";
            return false;
        }

        const AssetMeta meta = *metaPtr;

        EvictLoadedAssetCache(projectRelative);
        UncacheMeta(projectRelative);

        if (meta.AssetType == "Scene" && SceneManager::HasInstance())
        {
            SceneManager::Get().UnregisterScene(meta.AssetName);
        }

        return true;
    }

    bool AssetManager::MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError)
    {
        SuppressExternalSyncScope suppressScope;
        outError.clear();

        const std::string oldRel = NormalizeProjectRelativeAssetPath(oldPath);
        const std::string newRel = NormalizeProjectRelativeAssetPath(newPath);
        if (oldRel.empty() || newRel.empty())
        {
            outError = "invalid or out-of-project path";
            return false;
        }

        if (oldRel == newRel)
        {
            return true;
        }

        const AssetMeta* oldMetaPtr = FindAssetMetaByPath(oldRel);
        if (oldMetaPtr == nullptr)
        {
            outError = "asset not registered";
            return false;
        }

        if (FindAssetMetaByPath(newRel) != nullptr)
        {
            outError = "destination path is already registered";
            return false;
        }

        const std::filesystem::path oldExtension = std::filesystem::path(oldRel).extension();
        const std::filesystem::path newExtension = std::filesystem::path(newRel).extension();
        if (oldExtension != newExtension)
        {
            outError = "asset extension must not change when moving";
            return false;
        }

        const std::filesystem::path absoluteOld = ResolveAssetAbsolutePath(oldRel);
        const std::filesystem::path absoluteNew = ResolveAssetAbsolutePath(newRel);
        const std::filesystem::path metaAbsoluteOld = BuildMetaAbsolutePath(oldRel);
        const std::filesystem::path metaAbsoluteNew = BuildMetaAbsolutePath(newRel);

        const std::filesystem::path newParent = absoluteNew.parent_path();
        if (!std::filesystem::exists(newParent) || !std::filesystem::is_directory(newParent))
        {
            outError = "destination parent directory does not exist";
            return false;
        }

        if (std::filesystem::exists(absoluteNew))
        {
            outError = "destination file already exists";
            return false;
        }

        if (!std::filesystem::exists(absoluteOld))
        {
            outError = "source asset file does not exist";
            return false;
        }

        std::error_code renameError;
        std::filesystem::rename(absoluteOld, absoluteNew, renameError);
        if (renameError)
        {
            outError = "failed to rename asset file: " + renameError.message();
            return false;
        }

        bool metaRenamed = false;
        if (std::filesystem::exists(metaAbsoluteOld))
        {
            renameError.clear();
            std::filesystem::rename(metaAbsoluteOld, metaAbsoluteNew, renameError);
            if (renameError)
            {
                renameError.clear();
                std::filesystem::rename(absoluteNew, absoluteOld, renameError);
                outError = "failed to rename meta file: " + renameError.message();
                return false;
            }

            metaRenamed = true;
        }

        AssetMeta updatedMeta = *oldMetaPtr;
        if (!MoveRegistryEntry(oldRel, newRel, updatedMeta))
        {
            renameError.clear();
            std::filesystem::rename(absoluteNew, absoluteOld, renameError);
            if (metaRenamed)
            {
                renameError.clear();
                std::filesystem::rename(metaAbsoluteNew, metaAbsoluteOld, renameError);
            }

            outError = "failed to update asset registry";
            return false;
        }

        MoveLoadedAssetCacheKey(oldRel, newRel);

        if (!WriteMetaFile(updatedMeta))
        {
            ME_CORE_WARN("MoveAsset: meta file write failed after move to '{}'", newRel);
        }

        AssetRegistryChange change;
        change.Kind = AssetRegistryChangeKind::Moved;
        change.Guid = updatedMeta.Guid;
        change.OldPath = oldRel;
        change.NewPath = newRel;
        change.AssetTypeId = updatedMeta.AssetType;
        BroadcastChange(change);

        return true;
    }

    bool AssetManager::RenameAsset(const std::string& oldPath, const std::string& newFileName, std::string& outError)
    {
        SuppressExternalSyncScope suppressScope;
        outError.clear();

        if (newFileName.empty())
        {
            outError = "new file name must not be empty";
            return false;
        }

        if (newFileName.find('/') != std::string::npos || newFileName.find('\\') != std::string::npos)
        {
            outError = "new file name must not contain path separators";
            return false;
        }

        const std::string oldRel = NormalizeProjectRelativeAssetPath(oldPath);
        if (oldRel.empty())
        {
            outError = "invalid or out-of-project path";
            return false;
        }

        const std::filesystem::path newRelPath =
            std::filesystem::path(oldRel).parent_path() / newFileName;
        return MoveAsset(oldRel, newRelPath.lexically_normal().generic_string(), outError);
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
            auto iter = m_AssetRegistry.find(registryKey);
            if (iter != m_AssetRegistry.end())
            {
                return &iter->second;
            }
        }

        auto legacyIter = m_AssetRegistry.find(path);
        if (legacyIter != m_AssetRegistry.end())
        {
            return &legacyIter->second;
        }

        return nullptr;
    }

    const AssetMeta* AssetManager::FindAssetMetaByGuid(const GUID& guid) const
    {
        auto guidIter = m_AssetPathByGuid.find(guid);
        if (guidIter == m_AssetPathByGuid.end())
        {
            return nullptr;
        }

        auto pathIter = m_AssetRegistry.find(guidIter->second);
        if (pathIter == m_AssetRegistry.end())
        {
            return nullptr;
        }

        return &pathIter->second;
    }

    std::vector<const AssetMeta*> AssetManager::FindAssetMetasByType(const std::string& assetTypeId) const
    {
        std::vector<const AssetMeta*> result;
        auto bucketIter = m_AssetMetasByType.find(assetTypeId);
        if (bucketIter == m_AssetMetasByType.end())
        {
            return result;
        }

        result.reserve(bucketIter->second.size());
        for (AssetMeta* meta : bucketIter->second)
        {
            result.push_back(meta);
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

    std::shared_ptr<Asset> AssetManager::LoadAssetByMeta_Internal(const AssetMeta& meta, std::string& outErrorMessage)
    {
        if (meta.AssetType == "StaticMesh")
        {
            std::shared_ptr<StaticMesh> asset = LoadAsset<StaticMesh>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load static mesh by guid";
                return nullptr;
            }

            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "Texture2D")
        {
            std::shared_ptr<Texture2D> asset = LoadAsset<Texture2D>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load texture2d by guid";
                return nullptr;
            }

            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "Scene")
        {
            std::shared_ptr<Scene> asset = LoadAsset<Scene>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load scene by guid";
                return nullptr;
            }

            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "Material")
        {
            std::shared_ptr<Material> asset = LoadAsset<Material>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load material by guid";
                return nullptr;
            }
            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "Shader")
        {
            std::shared_ptr<Shader> asset = LoadAsset<Shader>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load shader by guid";
                return nullptr;
            }
            return std::static_pointer_cast<Asset>(asset);
        }

        outErrorMessage = "unsupported asset type '" + meta.AssetType + "'";
        return nullptr;
    }

    template<>
    std::shared_ptr<Shader> AssetManager::LoadAsset_Impl<Shader>(const AssetMeta& meta)
    {
        const std::string absoluteAssetPath = ResolveAssetAbsolutePathString(meta.AssetPath);

        ShaderResource resource;
        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            "minEngine::ShaderResource",
            &resource,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true});
        if (!result.ok)
        {
            ME_CORE_ERROR("Failed to deserialize shader resource '{}'. Error: {}. Field path: {}",
                          absoluteAssetPath,
                          result.message,
                          result.fieldPath);
            return nullptr;
        }
        std::shared_ptr<Shader> shader = NewObject<Shader>(meta.AssetName, nullptr, meta.Guid);
        std::string compileError;
        if (!shader->CompileFromFiles(
                *RenderSystem::Get().GetRHI(),
                resource.m_VertexPath,
                resource.m_FragmentPath,
                &compileError))
        {
            ME_CORE_ERROR(
                "Failed to compile shader asset '{}'. Vertex: '{}', Fragment: '{}'. {}",
                absoluteAssetPath,
                resource.m_VertexPath,
                resource.m_FragmentPath,
                compileError);
            return nullptr;
        }
        return shader;
    }

    template<>
    bool AssetManager::SaveAsset_Impl<Material>(const AssetMeta& meta, const Material& asset) const
    {
        const std::string absoluteAssetPath = ResolveAssetAbsolutePathString(meta.AssetPath);

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            "minEngine::Material",
            &asset,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true});

        if (!result.ok)
        {
            ME_CORE_ERROR("Failed to serialize material '{}'. Error: {}. Field path: {}",
                          absoluteAssetPath,
                          result.message,
                          result.fieldPath);
            return false;
        }

        return true;
    }

    template<>
    bool AssetManager::SaveAsset_Impl<Scene>(const AssetMeta& meta, const Scene& asset) const
    {
        const std::string absoluteAssetPath = ResolveAssetAbsolutePathString(meta.AssetPath);

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            "minEngine::Scene",
            &asset,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true});

        if (!result.ok)
        {
            ME_CORE_ERROR("Failed to serialize scene '{}'. Error: {}. Field path: {}",
                          absoluteAssetPath,
                          result.message,
                          result.fieldPath);
            return false;
        }

        return true;
    }
}
