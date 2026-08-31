#include "AssetManager.h"

#include "AssetTypeRegistry.h"
#include "Runtime/Resource/EditorFilesystemMutationPass.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Resource/Font.h"
#include "Runtime/Resource/LuaScript.h"
#include "Runtime/Resource/AudioClip.h"
#include "Runtime/Function/Render/Environment/EnvironmentMap.h"

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
            ME_CORE_WARN("Skip scanning assets because directory does not exist: {}", directory.string());
            return;
        }

        if (!std::filesystem::is_directory(directory))
        {
            ME_CORE_WARN("Skip scanning assets because path is not a directory: {}", directory.string());
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

        const std::filesystem::path absoluteAssetPath = ResolveAssetAbsolutePath(projectRelativePath);
        std::error_code fileError;
        if (!std::filesystem::exists(absoluteAssetPath, fileError)
            || !std::filesystem::is_regular_file(absoluteAssetPath, fileError))
        {
            std::string metaError;
            RemoveMetaFileOnDisk(projectRelativePath, metaError);
            if (!metaError.empty())
            {
                ME_CORE_WARN(
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

        const AssetMeta* existingGuidMeta = m_Registry.FindMetaByGuid(meta.Guid);
        if (existingGuidMeta != nullptr && existingGuidMeta->AssetPath != projectRelativePath)
        {
            ME_CORE_WARN(
                "GUID collision detected between '{}' and '{}'. Regenerating GUID for current asset.",
                existingGuidMeta->AssetPath,
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
        AssetRegistryBroadcastBatchScope batchScope;
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

        NoteEditorFilesystemMutation(absoluteDestDirectory);
        NoteEditorFilesystemMutation(destFilePath);

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

        NoteEditorFilesystemMutation(BuildMetaAbsolutePath(result.Meta.AssetPath));

        result.bSuccess = true;
        return result;
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
        const std::string newKey(newRel);
        inOutMeta.AssetPath = newKey;
        inOutMeta.AssetName = std::filesystem::path(newKey).stem().string();
        return m_Registry.MoveMeta(oldRel, newRel);
    }

    bool AssetManager::DeleteAsset(const std::string& assetPath, std::string& outError)
    {
        AssetRegistryBroadcastBatchScope batchScope;
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

        NoteEditorFilesystemMutation(absolutePath);
        NoteEditorFilesystemMutation(metaAbsolutePath);

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
                ME_CORE_WARN(
                    "RemoveOrphanMetaFilesInDirectory: failed to remove orphan meta '{}': {}",
                    metaPath.string(),
                    errorCode.message());
                continue;
            }

            ME_CORE_INFO(
                "RemoveOrphanMetaFilesInDirectory: removed orphan meta '{}'",
                metaPath.string());
        }
    }

    bool AssetManager::MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError)
    {
        AssetRegistryBroadcastBatchScope batchScope;
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

        NoteEditorFilesystemMutation(absoluteOld);
        NoteEditorFilesystemMutation(absoluteNew);
        NoteEditorFilesystemMutation(metaAbsoluteOld);
        NoteEditorFilesystemMutation(metaAbsoluteNew);
        NoteEditorFilesystemMutation(newParent);

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

        return true;
    }

    bool AssetManager::RenameAsset(const std::string& oldPath, const std::string& newFileName, std::string& outError)
    {
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
        const std::string_view assetTypeId = AssetTypeRegistry::Get().GetAssetTypeIdForClass(assetClass);
        if (assetTypeId.empty())
        {
            return {};
        }

        return FindAssetMetasByType(std::string(assetTypeId));
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
            outErrorMessage = "Shader assets are removed; use Material compile instead.";
            return nullptr;
        }

        if (meta.AssetType == "Font")
        {
            std::shared_ptr<Font> asset = LoadAsset<Font>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load font by guid";
                return nullptr;
            }
            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "LuaScript")
        {
            std::shared_ptr<LuaScript> asset = LoadAsset<LuaScript>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load lua script by guid";
                return nullptr;
            }
            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "EnvironmentMap")
        {
            std::shared_ptr<EnvironmentMap> asset = LoadAsset<EnvironmentMap>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load EnvironmentMap by guid";
                return nullptr;
            }
            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "AudioClip")
        {
            std::shared_ptr<AudioClip> asset = LoadAsset<AudioClip>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load AudioClip by guid";
                return nullptr;
            }
            return std::static_pointer_cast<Asset>(asset);
        }

        outErrorMessage = "unsupported asset type '" + meta.AssetType + "'";
        return nullptr;
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
