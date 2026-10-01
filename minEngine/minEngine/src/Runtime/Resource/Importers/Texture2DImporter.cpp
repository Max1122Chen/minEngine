#include "Runtime/Resource/Importers/Texture2DImporter.h"

#include "Runtime/Resource/Importers/AssetImportHelpers.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"
#include "Runtime/Core/Paths/PathRegistry.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace minEngine
{
    namespace
    {
        std::string NormalizeExtension(std::string_view extension)
        {
            std::string normalized(extension);
            for (char& ch : normalized)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return normalized;
        }

        std::string NormalizeChannelSemantics(std::string_view value)
        {
            if (value.empty())
            {
                return "Color";
            }
            if (value == "Color" || value == "SingleChannel" || value == "Mask")
            {
                return std::string(value);
            }
            return {};
        }

        bool PathContainsSourcesSegment(const std::filesystem::path& absolutePath)
        {
            for (const std::filesystem::path& part : absolutePath)
            {
                if (part == "Sources")
                {
                    return true;
                }
            }
            return false;
        }
    }

    bool Texture2DImporter::AcceptsSourceExtension(std::string_view extension)
    {
        const std::string normalized = NormalizeExtension(extension);
        return normalized == ".png" || normalized == ".jpg" || normalized == ".jpeg";
    }

    ImportResult Texture2DImporter::Import(AssetManager& manager, const ImportRequest& request)
    {
        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
        ImportAssetResult legacy;
        legacy.bSuccess = false;

        const std::filesystem::path& sourcePath = request.SourcePath;
        if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
        {
            legacy.ErrorMessage = "source file does not exist: " + sourcePath.string();
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        if (!AcceptsSourceExtension(sourcePath.extension().string()))
        {
            legacy.ErrorMessage =
                "Texture2D import expects .png/.jpg/.jpeg, got: " + sourcePath.extension().string();
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        const std::string semantics = NormalizeChannelSemantics(request.ChannelSemantics);
        if (semantics.empty())
        {
            legacy.ErrorMessage =
                "unknown channelSemantics '" + request.ChannelSemantics
                + "' (expected Color, SingleChannel, or Mask)";
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        if (contentRoot.empty())
        {
            legacy.ErrorMessage = "project content root is not set";
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        std::filesystem::path absoluteDestDirectory = request.DestDirectory.is_absolute()
            ? std::filesystem::weakly_canonical(request.DestDirectory)
            : std::filesystem::weakly_canonical(contentRoot / request.DestDirectory);

        if (!manager.IsUnderProjectContentRoot(absoluteDestDirectory))
        {
            legacy.ErrorMessage =
                "destination directory is outside project Assets: " + absoluteDestDirectory.string();
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        std::filesystem::path sourceCopyPath;
        const bool sourceAlreadyUnderContent =
            manager.IsUnderProjectContentRoot(sourcePath) && !PathContainsSourcesSegment(sourcePath);

        if (sourceAlreadyUnderContent)
        {
            // Auto-import / in-tree raw: keep the image and point SourcePath at it.
            sourceCopyPath = std::filesystem::weakly_canonical(sourcePath);
        }
        else
        {
            const std::filesystem::path sourcesDirectory = contentRoot / "Sources";
            std::error_code sourcesCreateError;
            std::filesystem::create_directories(sourcesDirectory, sourcesCreateError);
            if (sourcesCreateError)
            {
                legacy.ErrorMessage =
                    "failed to create Assets/Sources: " + sourcesCreateError.message();
                return AssetImportHelpers::MakeResultFromLegacy(legacy);
            }

            sourceCopyPath = sourcesDirectory / sourcePath.filename();
            if (!request.bOverwriteExisting
                && std::filesystem::exists(sourceCopyPath)
                && !std::filesystem::equivalent(sourceCopyPath, sourcePath))
            {
                legacy.ErrorMessage = "import source already exists: " + sourceCopyPath.string();
                return AssetImportHelpers::MakeResultFromLegacy(legacy);
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
                    legacy.ErrorMessage =
                        "failed to copy source into Assets/Sources: " + copySourceError.message();
                    return AssetImportHelpers::MakeResultFromLegacy(legacy);
                }
            }
            else if (
                request.bOverwriteExisting
                && !std::filesystem::equivalent(sourceCopyPath, sourcePath))
            {
                std::error_code copySourceError;
                std::filesystem::copy_file(
                    sourcePath,
                    sourceCopyPath,
                    std::filesystem::copy_options::overwrite_existing,
                    copySourceError);
                if (copySourceError)
                {
                    legacy.ErrorMessage =
                        "failed to refresh Assets/Sources copy: " + copySourceError.message();
                    return AssetImportHelpers::MakeResultFromLegacy(legacy);
                }
            }
        }

        std::filesystem::path productFileName = sourcePath.stem();
        productFileName += ".metex";
        const std::filesystem::path productPath = absoluteDestDirectory / productFileName;
        if (std::filesystem::exists(productPath) && !request.bOverwriteExisting)
        {
            legacy.ErrorMessage = "destination product already exists: " + productPath.string();
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        Texture2DCookSettings settings;
        if (request.bOverwriteExisting && std::filesystem::exists(productPath))
        {
            std::string readError;
            if (!Texture2DLoader::ReadCookSettings(productPath.string(), settings, &readError))
            {
                settings = Texture2DCookSettings{};
            }
        }

        if (!request.ChannelSemantics.empty())
        {
            settings.ChannelSemantics = semantics;
            settings.bSRGB = settings.ChannelSemantics == "Color";
        }
        else if (settings.ChannelSemantics.empty())
        {
            settings.ChannelSemantics = "Color";
            settings.bSRGB = true;
        }

        ImagePixels pixels;
        std::string loadError;
        if (!ImageLoader::Load(sourceCopyPath.string(), pixels, true, &loadError))
        {
            legacy.ErrorMessage = loadError.empty() ? "failed to decode source image" : loadError;
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        std::string semanticsError;
        if (!Texture2DLoader::ApplyChannelSemantics(pixels, settings, &semanticsError))
        {
            ImageLoader::Free(pixels);
            legacy.ErrorMessage = semanticsError;
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        std::error_code createDirError;
        std::filesystem::create_directories(absoluteDestDirectory, createDirError);
        if (createDirError)
        {
            ImageLoader::Free(pixels);
            legacy.ErrorMessage = "failed to create destination directory: " + createDirError.message();
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        manager.NoteEditorFilesystemMutation(absoluteDestDirectory);
        manager.NoteEditorFilesystemMutation(productPath);

        std::string writeError;
        if (!Texture2DLoader::WriteCookedFile(productPath.string(), settings, pixels, &writeError))
        {
            ImageLoader::Free(pixels);
            legacy.ErrorMessage = writeError;
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }
        ImageLoader::Free(pixels);

        legacy.Meta = manager.RegisterAsset(productPath.string(), "Texture2D");
        if (legacy.Meta.AssetPath.empty())
        {
            legacy.ErrorMessage = "failed to register cooked Texture2D asset";
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }

        legacy.Meta.SourcePath = manager.NormalizeProjectRelativeAssetPath(sourceCopyPath.string());
        if (!manager.WriteMetaFile(legacy.Meta))
        {
            legacy.ErrorMessage = "failed to write SourcePath into meta";
            return AssetImportHelpers::MakeResultFromLegacy(legacy);
        }
        manager.CacheMeta(legacy.Meta, true);
        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(legacy.Meta.AssetPath));

        if (request.bOverwriteExisting)
        {
            manager.EvictLoadedAssetCache(legacy.Meta.AssetPath);
        }

        legacy.bSuccess = true;
        return AssetImportHelpers::MakeResultFromLegacy(legacy);
    }

    bool Texture2DImporter::RecookFromMeta(
        AssetManager& manager,
        const AssetMeta& meta,
        std::string_view channelSemanticsOverride,
        std::string& outError)
    {
        outError.clear();
        if (meta.AssetType != "Texture2D")
        {
            outError = "RecookFromMeta expects Texture2D";
            return false;
        }
        if (meta.SourcePath.empty())
        {
            outError = "asset has no SourcePath (cannot recook): " + meta.AssetPath;
            return false;
        }

        ImportRequest request;
        request.SourcePath = manager.ResolveAssetAbsolutePath(meta.SourcePath);
        request.DestDirectory = manager.ResolveAssetAbsolutePath(meta.AssetPath).parent_path();
        request.ProductId = "Texture2D";
        request.bOverwriteExisting = true;
        request.ChannelSemantics = std::string(channelSemanticsOverride);

        const ImportResult result = Import(manager, request);
        if (!result.bSuccess)
        {
            outError = result.ErrorMessage.empty() ? "recook failed" : result.ErrorMessage;
            return false;
        }
        return true;
    }
}
