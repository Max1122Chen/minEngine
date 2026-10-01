#include "Runtime/Resource/Importers/MeshImporter.h"

#include "Runtime/Resource/Importers/AssetImportHelpers.h"
#include "Runtime/Resource/AssetTypeRegistry.h"
#include "Runtime/Resource/Loaders/AssimpMeshImportUtil.h"
#include "Runtime/Resource/Loaders/SkeletalMeshLoader.h"
#include "Runtime/Core/Paths/PathRegistry.h"

#include <system_error>

namespace minEngine
{
    bool MeshImporter::AcceptsSourceExtension(std::string_view extension)
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

    ImportResult MeshImporter::ImportStaticMesh(AssetManager& manager, const ImportRequest& request)
    {
        return AssetImportHelpers::MakeResultFromLegacy(
            Cook(
                manager,
                request.SourcePath,
                request.DestDirectory,
                MeshImportProductType::StaticMesh,
                request.bOverwriteExisting));
    }

    ImportResult MeshImporter::ImportSkeletalMesh(AssetManager& manager, const ImportRequest& request)
    {
        return AssetImportHelpers::MakeResultFromLegacy(
            Cook(
                manager,
                request.SourcePath,
                request.DestDirectory,
                MeshImportProductType::SkeletalMesh,
                request.bOverwriteExisting));
    }

    ImportAssetResult MeshImporter::Cook(
        AssetManager& manager,
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destDirectory,
        MeshImportProductType productType,
        bool bOverwriteExisting)
{
        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
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

        if (!manager.IsUnderProjectContentRoot(absoluteDestDirectory))
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

        manager.NoteEditorFilesystemMutation(sourcesDirectory);
        manager.NoteEditorFilesystemMutation(sourceCopyPath);

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

        manager.NoteEditorFilesystemMutation(absoluteDestDirectory);
        manager.NoteEditorFilesystemMutation(productPath);

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

        result.Meta = manager.RegisterAsset(productPath.string(), assetTypeId);
        if (result.Meta.AssetPath.empty())
        {
            result.ErrorMessage = "failed to register cooked mesh asset";
            return result;
        }

        result.Meta.SourcePath = manager.NormalizeProjectRelativeAssetPath(sourceCopyPath.string());
        if (!manager.WriteMetaFile(result.Meta))
        {
            result.ErrorMessage = "failed to write SourcePath into meta";
            return result;
        }
        manager.CacheMeta(result.Meta, true);

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(result.Meta.AssetPath));

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

            manager.NoteEditorFilesystemMutation(
                manager.ResolveAssetAbsolutePath(
                    SkeletalMeshLoader::BuildBuddyRelativePath(result.Meta.AssetPath)));
            manager.EvictLoadedAssetCache(
                SkeletalMeshLoader::BuildBuddyRelativePath(result.Meta.AssetPath));
            const std::filesystem::path meshPath(result.Meta.AssetPath);
            const std::string skeletonRelative =
                (meshPath.parent_path() / (meshPath.stem().string() + "_Skeleton.meskeleton"))
                    .generic_string();
            manager.EvictLoadedAssetCache(skeletonRelative);
        }

        if (bOverwriteExisting)
        {
            manager.EvictLoadedAssetCache(result.Meta.AssetPath);
        }

        result.bSuccess = true;
        return result;
    }

}
