#include "AssetManager.h"

#include "AssetPipelineBootstrap.h"
#include "AssetTypeRegistry.h"
#include "Runtime/Resource/EditorFilesystemMutationPass.h"
#include "Runtime/Resource/Loaders/AssimpMeshImportUtil.h"
#include "Runtime/Resource/Loaders/SkeletalMeshLoader.h"
#include "Runtime/Resource/Loaders/AnimationClipLoader.h"
#include "Runtime/Resource/Loaders/AnimationGraphLoader.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/SkeletalMesh.h"
#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Function/Animation/Skeleton.h"
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
            meta.Guid = GenerateGUID();

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
                meta.Guid = GenerateGUID();
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

    ImportAssetResult AssetManager::ImportAsset(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destDirectory,
        bool bOverwriteExisting)
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
        std::string extension = sourcePath.extension().string();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (extension == ".fbx" || extension == ".gltf")
        {
            result.ErrorMessage =
                "external mesh source requires ImportExternalMesh (choose StaticMesh or SkeletalMesh): "
                + sourcePath.extension().string();
            return result;
        }

        const std::string assetTypeId = typeRegistry.InferAssetTypeFromExtension(sourcePath);
        if (assetTypeId.empty())
        {
            result.ErrorMessage = "unsupported file extension: " + extension;
            return result;
        }

        const std::filesystem::path destFilePath = absoluteDestDirectory / sourcePath.filename();
        if (std::filesystem::exists(destFilePath) && !bOverwriteExisting)
        {
            result.ErrorMessage = "destination file already exists: " + destFilePath.string();
            return result;
        }

        NoteEditorFilesystemMutation(absoluteDestDirectory);
        NoteEditorFilesystemMutation(destFilePath);

        const std::filesystem::copy_options copyOptions = bOverwriteExisting
            ? std::filesystem::copy_options::overwrite_existing
            : std::filesystem::copy_options::none;
        std::error_code copyError;
        std::filesystem::copy_file(sourcePath, destFilePath, copyOptions, copyError);
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

        if (bOverwriteExisting)
        {
            EvictLoadedAssetCache(result.Meta.AssetPath);
        }

        NoteEditorFilesystemMutation(BuildMetaAbsolutePath(result.Meta.AssetPath));

        result.bSuccess = true;
        return result;
    }

    ImportAssetResult AssetManager::ImportExternalMesh(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destDirectory,
        MeshImportProductType productType,
        bool bOverwriteExisting)
    {
        AssetRegistryBroadcastBatchScope batchScope;
        ImportAssetResult result;

        if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
        {
            result.ErrorMessage = "source file does not exist: " + sourcePath.string();
            return result;
        }

        if (!AssetTypeRegistry::IsExternalMeshSourceExtension(sourcePath.extension().string())
            && sourcePath.extension() != ".obj")
        {
            result.ErrorMessage =
                "ImportExternalMesh expects .fbx/.gltf/.glb/.obj, got: "
                + sourcePath.extension().string();
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
            result.ErrorMessage =
                "destination directory is outside project Assets: " + absoluteDestDirectory.string();
            return result;
        }

        const std::filesystem::path sourcesDirectory = contentRoot / "Sources";
        std::error_code sourcesCreateError;
        std::filesystem::create_directories(sourcesDirectory, sourcesCreateError);
        if (sourcesCreateError)
        {
            result.ErrorMessage =
                "failed to create Assets/Sources: " + sourcesCreateError.message();
            return result;
        }

        const std::filesystem::path sourceCopyPath = sourcesDirectory / sourcePath.filename();
        if (!bOverwriteExisting
            && std::filesystem::exists(sourceCopyPath)
            && !std::filesystem::equivalent(sourceCopyPath, sourcePath))
        {
            // Allow re-import when the picker already pointed at Assets/Sources/file.
            result.ErrorMessage = "import source already exists: " + sourceCopyPath.string();
            return result;
        }

        NoteEditorFilesystemMutation(sourcesDirectory);
        NoteEditorFilesystemMutation(sourceCopyPath);

        if (!std::filesystem::exists(sourceCopyPath))
        {
            std::error_code copySourceError;
            std::filesystem::copy_file(
                sourcePath,
                sourceCopyPath,
                std::filesystem::copy_options::none,
                copySourceError);
            if (copySourceError)
            {
                result.ErrorMessage = "failed to copy source into Assets/Sources: "
                    + copySourceError.message();
                return result;
            }
        }
        else if (bOverwriteExisting
            && !std::filesystem::equivalent(sourceCopyPath, sourcePath))
        {
            // Reimport from a path that is not the Sources copy: refresh Sources from that path.
            std::error_code copySourceError;
            std::filesystem::copy_file(
                sourcePath,
                sourceCopyPath,
                std::filesystem::copy_options::overwrite_existing,
                copySourceError);
            if (copySourceError)
            {
                result.ErrorMessage = "failed to refresh Assets/Sources copy: "
                    + copySourceError.message();
                return result;
            }
        }

        const bool skeletal = productType == MeshImportProductType::SkeletalMesh;
        const char* assetTypeId = skeletal ? "SkeletalMesh" : "StaticMesh";
        const char* productExtension = skeletal ? ".glb" : ".obj";
        const char* exportFormatId = skeletal ? "glb2" : "obj";

        std::filesystem::path productFileName = sourcePath.stem();
        productFileName += productExtension;
        const std::filesystem::path productPath = absoluteDestDirectory / productFileName;
        if (std::filesystem::exists(productPath) && !bOverwriteExisting)
        {
            result.ErrorMessage = "destination product already exists: " + productPath.string();
            return result;
        }

        NoteEditorFilesystemMutation(absoluteDestDirectory);
        NoteEditorFilesystemMutation(productPath);

        std::string cookError;
        if (!AssimpMeshImportUtil::CookExternalMeshToFile(
                sourceCopyPath,
                productPath,
                exportFormatId,
                &cookError))
        {
            result.ErrorMessage = cookError;
            return result;
        }

        result.Meta = RegisterAsset(productPath.string(), assetTypeId);
        if (result.Meta.AssetPath.empty())
        {
            result.ErrorMessage = "failed to register cooked mesh asset";
            return result;
        }

        result.Meta.SourcePath = NormalizeProjectRelativeAssetPath(sourceCopyPath.string());
        if (!WriteMetaFile(result.Meta))
        {
            result.ErrorMessage = "failed to write SourcePath into meta";
            return result;
        }
        CacheMeta(result.Meta, true);

        NoteEditorFilesystemMutation(BuildMetaAbsolutePath(result.Meta.AssetPath));

        if (skeletal)
        {
            std::string skeletalCookError;
            if (!SkeletalMeshLoader::FinishSkeletalImportCook(
                    result.Meta,
                    sourceCopyPath,
                    &skeletalCookError))
            {
                result.ErrorMessage = skeletalCookError;
                return result;
            }

            NoteEditorFilesystemMutation(
                ResolveAssetAbsolutePath(
                    SkeletalMeshLoader::BuildBuddyRelativePath(result.Meta.AssetPath)));
            EvictLoadedAssetCache(
                SkeletalMeshLoader::BuildBuddyRelativePath(result.Meta.AssetPath));
            const std::filesystem::path meshPath(result.Meta.AssetPath);
            const std::string skeletonRelative =
                (meshPath.parent_path() / (meshPath.stem().string() + "_Skeleton.meskeleton"))
                    .generic_string();
            EvictLoadedAssetCache(skeletonRelative);
        }

        if (bOverwriteExisting)
        {
            EvictLoadedAssetCache(result.Meta.AssetPath);
        }

        result.bSuccess = true;
        return result;
    }

    ImportAssetResult AssetManager::ImportAnimationClip(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destDirectory,
        std::string_view skeletonAssetPath,
        int animationIndex,
        bool bOverwriteExisting)
    {
        AssetRegistryBroadcastBatchScope batchScope;
        ImportAssetResult result;

        if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
        {
            result.ErrorMessage = "source file does not exist: " + sourcePath.string();
            return result;
        }

        if (!AssetTypeRegistry::IsExternalMeshSourceExtension(sourcePath.extension().string()))
        {
            result.ErrorMessage =
                "ImportAnimationClip expects .fbx/.gltf/.glb, got: " + sourcePath.extension().string();
            return result;
        }

        if (skeletonAssetPath.empty())
        {
            result.ErrorMessage = "skeletonAssetPath is empty";
            return result;
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        if (contentRoot.empty())
        {
            result.ErrorMessage = "project content root is not set";
            return result;
        }

        std::shared_ptr<Skeleton> skeleton = LoadAsset<Skeleton>(std::string(skeletonAssetPath));
        if (skeleton == nullptr)
        {
            result.ErrorMessage = "failed to load Skeleton: " + std::string(skeletonAssetPath);
            return result;
        }

        std::filesystem::path absoluteDestDirectory = destDirectory.is_absolute()
            ? std::filesystem::weakly_canonical(destDirectory)
            : std::filesystem::weakly_canonical(contentRoot / destDirectory);

        if (!IsUnderProjectContentRoot(absoluteDestDirectory))
        {
            result.ErrorMessage =
                "destination directory is outside project Assets: " + absoluteDestDirectory.string();
            return result;
        }

        const std::filesystem::path sourcesDirectory = contentRoot / "Sources";
        std::error_code sourcesCreateError;
        std::filesystem::create_directories(sourcesDirectory, sourcesCreateError);
        if (sourcesCreateError)
        {
            result.ErrorMessage = "failed to create Assets/Sources: " + sourcesCreateError.message();
            return result;
        }

        const std::filesystem::path sourceCopyPath = sourcesDirectory / sourcePath.filename();
        NoteEditorFilesystemMutation(sourcesDirectory);
        NoteEditorFilesystemMutation(sourceCopyPath);

        if (!std::filesystem::exists(sourceCopyPath))
        {
            std::error_code copySourceError;
            std::filesystem::copy_file(
                sourcePath,
                sourceCopyPath,
                std::filesystem::copy_options::none,
                copySourceError);
            if (copySourceError)
            {
                result.ErrorMessage =
                    "failed to copy source into Assets/Sources: " + copySourceError.message();
                return result;
            }
        }
        else if (!std::filesystem::equivalent(sourceCopyPath, sourcePath))
        {
            if (bOverwriteExisting)
            {
                std::error_code copySourceError;
                std::filesystem::copy_file(
                    sourcePath,
                    sourceCopyPath,
                    std::filesystem::copy_options::overwrite_existing,
                    copySourceError);
                if (copySourceError)
                {
                    result.ErrorMessage =
                        "failed to refresh Assets/Sources copy: " + copySourceError.message();
                    return result;
                }
            }
            else
            {
                ME_LOG(LogAsset, Info, 
                    "ImportAnimationClip: reusing existing Sources copy '{}'",
                    sourceCopyPath.string());
            }
        }

        AnimationClip importedClip;
        std::string importError;
        if (!AnimationClipLoader::ImportFromFile(
                sourceCopyPath.string(),
                skeleton,
                animationIndex,
                importedClip,
                &importError))
        {
            result.ErrorMessage = importError;
            return result;
        }

        std::filesystem::path productFileName = sourcePath.stem();
        if (animationIndex > 0)
        {
            productFileName += "_";
            productFileName += std::to_string(animationIndex);
        }
        productFileName += ".meaclip";
        const std::filesystem::path productPath = absoluteDestDirectory / productFileName;
        if (std::filesystem::exists(productPath) && !bOverwriteExisting)
        {
            result.ErrorMessage = "destination clip already exists: " + productPath.string();
            return result;
        }

        NoteEditorFilesystemMutation(absoluteDestDirectory);
        NoteEditorFilesystemMutation(productPath);

        // Write clip + meta with a stable Guid before RegisterAsset (same order as Skeleton cook).
        AssetMeta pendingMeta;
        pendingMeta.AssetName = productFileName.stem().string();
        pendingMeta.AssetPath =
            NormalizeProjectRelativeAssetPath(productPath.string());
        pendingMeta.AssetType = "AnimationClip";
        pendingMeta.Guid = GenerateGUID();
        if (pendingMeta.AssetPath.empty())
        {
            result.ErrorMessage = "failed to resolve AnimationClip destination path";
            return result;
        }

        if (bOverwriteExisting)
        {
            if (const AssetMeta* existingMeta = FindAssetMetaByPath(pendingMeta.AssetPath))
            {
                pendingMeta.Guid = existingMeta->Guid;
                if (!existingMeta->SourcePath.empty())
                {
                    pendingMeta.SourcePath = existingMeta->SourcePath;
                }
            }
        }

        importedClip.SetName(pendingMeta.AssetName);
        importedClip.SetGuid(pendingMeta.Guid);

        std::string saveError;
        if (!AnimationClipLoader::Save(pendingMeta, importedClip, &saveError))
        {
            result.ErrorMessage = saveError;
            return result;
        }

        if (!WriteOrUpdateMetaFile(pendingMeta))
        {
            result.ErrorMessage = "failed to write AnimationClip meta";
            return result;
        }

        result.Meta = RegisterAsset(productPath.string(), "AnimationClip");
        if (result.Meta.AssetPath.empty())
        {
            result.ErrorMessage = "failed to register AnimationClip asset";
            return result;
        }

        result.Meta.SourcePath = NormalizeProjectRelativeAssetPath(sourceCopyPath.string());
        if (!WriteMetaFile(result.Meta))
        {
            result.ErrorMessage = "failed to write SourcePath into meta";
            return result;
        }
        CacheMeta(result.Meta, true);
        NoteEditorFilesystemMutation(BuildMetaAbsolutePath(result.Meta.AssetPath));

        if (bOverwriteExisting)
        {
            EvictLoadedAssetCache(result.Meta.AssetPath);
        }

        result.bSuccess = true;
        return result;
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
            ME_LOG(LogAsset, Warn, "MoveAsset: meta file write failed after move to '{}'", newRel);
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

    ImportResult AssetManager::MakeImportResultFromLegacy(const ImportAssetResult& legacy)
    {
        ImportResult result;
        result.bSuccess = legacy.bSuccess;
        result.ErrorMessage = legacy.ErrorMessage;
        if (legacy.bSuccess && !legacy.Meta.AssetPath.empty())
        {
            result.Created.push_back(ImportCreatedAsset{
                .AssetPath = legacy.Meta.AssetPath,
                .AssetTypeId = legacy.Meta.AssetType,
                .Guid = legacy.Meta.Guid});
        }

        return result;
    }

    bool AssetManager::AcceptsNativeCopyExtension(std::string_view extension)
    {
        if (AssetTypeRegistry::IsExternalMeshSourceExtension(extension))
        {
            return false;
        }

        std::string normalized(extension);
        for (char& ch : normalized)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        // .obj stays cookable via StaticMesh product; NativeCopy still accepts other registered
        // extensions (textures, .memtl, …). Infer rejects unknown extensions at Import time.
        return !normalized.empty();
    }

    bool AssetManager::AcceptsExternalMeshCookExtension(std::string_view extension)
    {
        if (AssetTypeRegistry::IsExternalMeshSourceExtension(extension))
        {
            return true;
        }

        std::string normalized(extension);
        for (char& ch : normalized)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        return normalized == ".obj";
    }

    bool AssetManager::AcceptsAnimationClipSourceExtension(std::string_view extension)
    {
        return AssetTypeRegistry::IsExternalMeshSourceExtension(extension);
    }

    ImportResult AssetManager::ImportProduct_NativeCopy(
        AssetManager& manager, const ImportRequest& request)
    {
        return MakeImportResultFromLegacy(
            manager.ImportAsset(
                request.SourcePath, request.DestDirectory, request.bOverwriteExisting));
    }

    ImportResult AssetManager::ImportProduct_StaticMesh(
        AssetManager& manager, const ImportRequest& request)
    {
        return MakeImportResultFromLegacy(manager.ImportExternalMesh(
            request.SourcePath,
            request.DestDirectory,
            MeshImportProductType::StaticMesh,
            request.bOverwriteExisting));
    }

    ImportResult AssetManager::ImportProduct_SkeletalMesh(
        AssetManager& manager, const ImportRequest& request)
    {
        return MakeImportResultFromLegacy(manager.ImportExternalMesh(
            request.SourcePath,
            request.DestDirectory,
            MeshImportProductType::SkeletalMesh,
            request.bOverwriteExisting));
    }

    ImportResult AssetManager::ImportProduct_AnimationClip(
        AssetManager& manager, const ImportRequest& request)
    {
        std::string skeletonAssetPath = request.SkeletonAssetPath;
        if (skeletonAssetPath.empty())
        {
            const PathRegistry& paths = PathRegistry::Get();
            const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
            std::filesystem::path absoluteDestDirectory = request.DestDirectory.is_absolute()
                ? std::filesystem::weakly_canonical(request.DestDirectory)
                : std::filesystem::weakly_canonical(contentRoot / request.DestDirectory);

            const std::filesystem::path fallbackAbsolute =
                absoluteDestDirectory
                / (request.SourcePath.stem().string() + "_Skeleton.meskeleton");
            if (std::filesystem::exists(fallbackAbsolute))
            {
                std::error_code relativeError;
                const std::filesystem::path relativePath =
                    std::filesystem::relative(fallbackAbsolute, contentRoot, relativeError);
                if (!relativeError && !relativePath.empty())
                {
                    skeletonAssetPath = relativePath.generic_string();
                }
            }
        }

        if (skeletonAssetPath.empty())
        {
            ImportResult result;
            result.ErrorMessage =
                "AnimationClip import requires SkeletonAssetPath "
                "(or an existing {stem}_Skeleton.meskeleton next to the destination)";
            return result;
        }

        return MakeImportResultFromLegacy(manager.ImportAnimationClip(
            request.SourcePath,
            request.DestDirectory,
            skeletonAssetPath,
            request.AnimationIndex,
            request.bOverwriteExisting));
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
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<StaticMesh>(
            meta, outErrorMessage, "failed to load static mesh by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_SkeletalMesh(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<SkeletalMesh>(
            meta, outErrorMessage, "failed to load skeletal mesh by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Skeleton(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<Skeleton>(
            meta, outErrorMessage, "failed to load Skeleton");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_AnimationClip(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<AnimationClip>(
            meta, outErrorMessage, "failed to load AnimationClip");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_AnimationGraph(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<AnimationGraph>(
            meta, outErrorMessage, "failed to load AnimationGraph");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Texture2D(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<Texture2D>(
            meta, outErrorMessage, "failed to load texture2d by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Scene(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<Scene>(
            meta, outErrorMessage, "failed to load scene by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Material(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<Material>(
            meta, outErrorMessage, "failed to load material by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_Font(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<Font>(
            meta, outErrorMessage, "failed to load font by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_LuaScript(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<LuaScript>(
            meta, outErrorMessage, "failed to load lua script by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_EnvironmentMap(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<EnvironmentMap>(
            meta, outErrorMessage, "failed to load EnvironmentMap by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_AudioClip(
        AssetManager& manager, const AssetMeta& meta, std::string& outErrorMessage)
    {
        return manager.LoadTypedAssetAsBase<AudioClip>(
            meta, outErrorMessage, "failed to load AudioClip by guid");
    }

    std::shared_ptr<Asset> AssetManager::LoadHandler_ShaderRemoved(
        AssetManager& /*manager*/, const AssetMeta& /*meta*/, std::string& outErrorMessage)
    {
        outErrorMessage = "Shader assets are removed; use Material compile instead.";
        return nullptr;
    }

    std::shared_ptr<Asset> AssetManager::LoadAssetByMeta_Internal(const AssetMeta& meta, std::string& outErrorMessage)
    {
        const AssetLoadHandlerFn handler = FindLoadHandler(meta.AssetType);
        if (handler == nullptr)
        {
            outErrorMessage = "unsupported asset type '" + meta.AssetType + "'";
            return nullptr;
        }

        return handler(*this, meta, outErrorMessage);
    }

    template<>
    bool AssetManager::SaveAsset_Impl<Material>(const AssetMeta& meta, const Material& asset) const
    {
        const std::string absoluteAssetPath = ResolveAssetAbsolutePathString(meta.AssetPath);

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            &asset,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = false});

        if (!result.ok)
        {
            ME_LOG(LogAsset, Error, "Failed to serialize material '{}'. Error: {}. Field path: {}",
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
            &asset,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = false});

        if (!result.ok)
        {
            ME_LOG(LogAsset, Error, "Failed to serialize scene '{}'. Error: {}. Field path: {}",
                          absoluteAssetPath,
                          result.message,
                          result.fieldPath);
            return false;
        }

        return true;
    }

    namespace
    {
        std::string SanitizeAssetBaseName(std::string baseName)
        {
            if (baseName.empty())
            {
                return "NewAsset";
            }

            for (char& character : baseName)
            {
                const bool allowed = std::isalnum(static_cast<unsigned char>(character)) != 0
                    || character == '_' || character == '-';
                if (!allowed)
                {
                    character = '_';
                }
            }

            return baseName;
        }

        std::string BuildUniqueProjectRelativeAssetPath(
            const AssetManager& assetManager,
            std::string_view directoryRel,
            std::string_view baseName,
            std::string_view extension)
        {
            std::string directory = std::string(directoryRel);
            if (directory.empty())
            {
                directory = "Assets";
            }

            const std::string sanitizedBase = SanitizeAssetBaseName(std::string(baseName));
            const std::filesystem::path directoryPath(directory);

            for (int suffixIndex = 0; suffixIndex < 1000; ++suffixIndex)
            {
                const std::string candidateBase = suffixIndex == 0
                    ? sanitizedBase
                    : sanitizedBase + "_" + std::to_string(suffixIndex);
                const std::filesystem::path relativePath =
                    (directoryPath / (candidateBase + std::string(extension))).lexically_normal();
                const std::string normalizedPath = relativePath.generic_string();

                if (assetManager.FindAssetMetaByPath(normalizedPath) != nullptr)
                {
                    continue;
                }

                const std::filesystem::path absolutePath =
                    assetManager.ResolveAssetAbsolutePath(normalizedPath);
                std::error_code fileError;
                if (!std::filesystem::exists(absolutePath, fileError))
                {
                    return normalizedPath;
                }
            }

            return std::string();
        }

        bool WriteSceneAssetFile(const AssetManager& assetManager, const std::string& relativePath, const Scene& scene)
        {
            const std::string absoluteAssetPath = assetManager.ResolveAssetAbsolutePath(relativePath).string();

            Serialization::JsonWriterArchive archive;
            const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
                absoluteAssetPath,
                &scene,
                archive,
                Serialization::SerializerOptions{
                    .enumAsString = true,
                    .strictTypeCheck = false,
                    .skipUnknownField = false});

            if (!result.ok)
            {
                ME_LOG(LogAsset, Error, 
                    "CreateAsset<Scene>: failed to serialize '{}'. Error: {}. Field path: {}",
                    relativePath,
                    result.message,
                    result.fieldPath);
                return false;
            }

            return true;
        }

        bool WriteMaterialAssetFile(
            const AssetManager& assetManager,
            const std::string& relativePath,
            const Material& material)
        {
            const std::string absoluteAssetPath = assetManager.ResolveAssetAbsolutePath(relativePath).string();

            Serialization::JsonWriterArchive archive;
            const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
                absoluteAssetPath,
                &material,
                archive,
                Serialization::SerializerOptions{
                    .enumAsString = true,
                    .strictTypeCheck = false,
                    .skipUnknownField = false});

            if (!result.ok)
            {
                ME_LOG(LogAsset, Error, 
                    "CreateAsset<Material>: failed to serialize '{}'. Error: {}. Field path: {}",
                    relativePath,
                    result.message,
                    result.fieldPath);
                return false;
            }

            return true;
        }
    }

    template<>
    std::shared_ptr<Scene> AssetManager::CreateAsset<Scene>(
        const std::string& assetName,
        const std::string& directoryRel)
    {
        const std::string relativePath =
            BuildUniqueProjectRelativeAssetPath(*this, directoryRel, assetName, ".mescene");
        if (relativePath.empty())
        {
            ME_LOG(LogAsset, Error, "CreateAsset<Scene>: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error, 
                "CreateAsset<Scene>: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string sceneName = absolutePath.stem().string();
        std::shared_ptr<Scene> scene = NewObject<Scene>(sceneName, nullptr, GenerateGUID());
        scene->Reset();
        scene->SetSceneName(sceneName);
        scene->EnsureRenderScene();

        if (!WriteSceneAssetFile(*this, relativePath, *scene))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return nullptr;
        }

        NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = RegisterAsset(relativePath, "Scene");
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "CreateAsset<Scene>: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        if (SceneManager::HasInstance())
        {
            SceneManager::Get().RegisterScene(meta.AssetName, meta.AssetPath);
        }

        NoteEditorFilesystemMutation(BuildMetaAbsolutePath(meta.AssetPath));

        ME_LOG(LogAsset, Info, "CreateAsset<Scene>: created '{}'.", meta.AssetPath);
        return LoadAsset<Scene>(meta.AssetPath);
    }

    template<>
    std::shared_ptr<Material> AssetManager::CreateAsset<Material>(
        const std::string& assetName,
        const std::string& directoryRel)
    {
        const std::string relativePath =
            BuildUniqueProjectRelativeAssetPath(*this, directoryRel, assetName, ".memtl");
        if (relativePath.empty())
        {
            ME_LOG(LogAsset, Error, "CreateAsset<Material>: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error, 
                "CreateAsset<Material>: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string materialName = absolutePath.stem().string();
        std::shared_ptr<Material> material = NewObject<Material>(materialName, nullptr, GenerateGUID());
        material->m_ShadingModel = MaterialShadingModel::Unlit;

        if (!WriteMaterialAssetFile(*this, relativePath, *material))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return nullptr;
        }

        NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = RegisterAsset(relativePath, "Material");
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "CreateAsset<Material>: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        NoteEditorFilesystemMutation(BuildMetaAbsolutePath(meta.AssetPath));

        ME_LOG(LogAsset, Info, "CreateAsset<Material>: created '{}'.", meta.AssetPath);
        return LoadAsset<Material>(meta.AssetPath);
    }
    template<>
    bool AssetManager::SaveAsset_Impl<AnimationGraph>(const AssetMeta& meta, const AnimationGraph& asset) const
    {
        std::string error;
        if (!AnimationGraphLoader::Save(meta, asset, &error))
        {
            ME_LOG(LogAsset, Error, 
                "Failed to save AnimationGraph '{}'. Error: {}",
                meta.AssetPath,
                error);
            return false;
        }
        return true;
    }

    template<>
    std::shared_ptr<AnimationGraph> AssetManager::CreateAsset<AnimationGraph>(
        const std::string& assetName,
        const std::string& directoryRel)
    {
        const std::string relativePath =
            BuildUniqueProjectRelativeAssetPath(*this, directoryRel, assetName, ".meagraph");
        if (relativePath.empty())
        {
            ME_LOG(LogAsset, Error, "CreateAsset<AnimationGraph>: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error, 
                "CreateAsset<AnimationGraph>: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string graphName = absolutePath.stem().string();
        std::shared_ptr<AnimationGraph> graph = NewObject<AnimationGraph>(graphName, nullptr, GenerateGUID());

        AssetMeta tempMeta;
        tempMeta.AssetPath = relativePath;
        tempMeta.AssetName = graphName;
        tempMeta.AssetType = "AnimationGraph";
        tempMeta.Guid = graph->GetGuid();

        std::string saveError;
        if (!AnimationGraphLoader::Save(tempMeta, *graph, &saveError))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            ME_LOG(LogAsset, Error, "CreateAsset<AnimationGraph>: save failed: {}", saveError);
            return nullptr;
        }

        NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = RegisterAsset(relativePath, "AnimationGraph");
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "CreateAsset<AnimationGraph>: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        NoteEditorFilesystemMutation(BuildMetaAbsolutePath(meta.AssetPath));

        ME_LOG(LogAsset, Info, "CreateAsset<AnimationGraph>: created '{}'.", meta.AssetPath);
        return LoadAsset<AnimationGraph>(meta.AssetPath);
    }

}
