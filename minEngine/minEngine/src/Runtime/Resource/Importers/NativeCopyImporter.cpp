#include "Runtime/Resource/Importers/NativeCopyImporter.h"

#include "Runtime/Resource/Importers/AssetImportHelpers.h"
#include "Runtime/Resource/AssetTypeRegistry.h"
#include "Runtime/Core/Paths/PathRegistry.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace minEngine
{
    bool NativeCopyImporter::AcceptsSourceExtension(std::string_view extension)
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

        // images are Texture2D Product only (never NativeCopy identity).
        if (normalized == ".png" || normalized == ".jpg" || normalized == ".jpeg")
        {
            return false;
        }

        return !normalized.empty();
    }

    ImportResult NativeCopyImporter::Import(AssetManager& manager, const ImportRequest& request)
    {
        return AssetImportHelpers::MakeResultFromLegacy(
            Cook(manager, request.SourcePath, request.DestDirectory, request.bOverwriteExisting));
    }

    ImportAssetResult NativeCopyImporter::Cook(
        AssetManager& manager,
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destDirectory,
        bool bOverwriteExisting)
{
        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
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

        if (!manager.IsUnderProjectContentRoot(absoluteDestDirectory))
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

        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
        {
            result.ErrorMessage =
                "image sources require Texture2D import product (cooks .metex), got: " + extension;
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

        manager.NoteEditorFilesystemMutation(absoluteDestDirectory);
        manager.NoteEditorFilesystemMutation(destFilePath);

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

        result.Meta = manager.RegisterAsset(destFilePath.string(), assetTypeId);
        if (result.Meta.AssetPath.empty())
        {
            result.ErrorMessage = "failed to register imported asset";
            return result;
        }

        if (bOverwriteExisting)
        {
            manager.EvictLoadedAssetCache(result.Meta.AssetPath);
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(result.Meta.AssetPath));

        result.bSuccess = true;
        return result;
    }

}
