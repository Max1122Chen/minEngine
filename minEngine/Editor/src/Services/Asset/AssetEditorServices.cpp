#include "Services/Asset/AssetEditorServices.h"

#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Resource/Asset.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Core/Paths/PathRegistry.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace minEngine
{
    AssetEditorResult AssetEditorServices::OkResult()
    {
        AssetEditorResult result;
        result.Ok = true;
        return result;
    }

    AssetEditorResult AssetEditorServices::Fail(std::string message)
    {
        AssetEditorResult result;
        result.Ok = false;
        result.Message = std::move(message);
        return result;
    }

    AssetEditorResult AssetEditorServices::List(
        std::string_view directory,
        std::string_view assetType,
        std::vector<const AssetMeta*>& outMetas) const
    {
        outMetas.clear();
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }

        AssetManager& assets = AssetManager::Get();
        const std::string directoryStorage = directory.empty() ? std::string("Assets") : std::string(directory);
        std::vector<const AssetMeta*> found = assetType.empty()
            ? assets.FindAssetMetasUnderDirectory(directoryStorage)
            : assets.FindAssetMetasByType(std::string(assetType));

        if (!assetType.empty() && !directory.empty())
        {
            const std::string prefix = directoryStorage;
            found.erase(
                std::remove_if(
                    found.begin(),
                    found.end(),
                    [&](const AssetMeta* meta)
                    {
                        if (meta == nullptr)
                        {
                            return true;
                        }
                        return meta->AssetPath.rfind(prefix, 0) != 0;
                    }),
                found.end());
        }

        outMetas = std::move(found);
        return OkResult();
    }

    bool AssetEditorServices::IsCreatableAssetTypeId(std::string_view assetTypeId)
    {
        static constexpr std::string_view kCreatableAssetTypeIds[] = {
            "Scene",
            "Material",
            "MaterialInstance",
            "AnimationGraph",
            "Sprite",
            "SpriteAtlas",
            "Texture2D",
        };

        for (const std::string_view allowed : kCreatableAssetTypeIds)
        {
            if (assetTypeId == allowed)
            {
                return true;
            }
        }

        return false;
    }

    AssetEditorResult AssetEditorServices::Create(
        std::string_view assetType,
        std::string_view directory,
        std::string_view name,
        std::string& outPath,
        std::string& outGuid)
    {
        outPath.clear();
        outGuid.clear();
        if (directory.empty() || name.empty() || assetType.empty())
        {
            return Fail("assetType, directory, and name are required.");
        }
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }

        const std::string typeStorage(assetType);
        if (!IsCreatableAssetTypeId(typeStorage))
        {
            return Fail("assetType '" + typeStorage + "' is not in the create whitelist.");
        }

        std::string createError;
        const std::shared_ptr<Asset> created = AssetManager::Get().CreateAssetByTypeId(
            typeStorage,
            std::string(name),
            std::string(directory),
            createError);
        if (created == nullptr)
        {
            return Fail(createError.empty() ? "CreateAsset failed." : createError);
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByGuid(created->GetGuid());
        if (meta == nullptr)
        {
            return Fail("Created asset has no registry meta.");
        }

        outPath = meta->AssetPath;
        outGuid = meta->Guid.ToString();
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::Delete(
        IEditorContext& context,
        std::string_view assetPath,
        bool unpackOpenPrefabInstanceRefs)
    {
        if (assetPath.empty())
        {
            return Fail("path is required.");
        }
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }

        const std::string pathStorage(assetPath);
        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(pathStorage);
        if (meta == nullptr)
        {
            return Fail("Asset not found.");
        }

        if (meta->AssetType == "Prefab")
        {
            if (SceneEditor* sceneEditor = GetSceneEditor(&context))
            {
                if (sceneEditor->GetPrefabStages().HasStage(pathStorage))
                {
                    return Fail("Close the Prefab stage before deleting this asset.");
                }
            }
        }

        std::string errorMessage;
        if (!AssetManager::Get().DeleteAsset(pathStorage, errorMessage, unpackOpenPrefabInstanceRefs))
        {
            return Fail(errorMessage.empty() ? "DeleteAsset failed." : errorMessage);
        }
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::Reimport(std::string_view assetPath)
    {
        if (assetPath.empty())
        {
            return Fail("path is required.");
        }
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }

        std::string error;
        if (!AssetManager::Get().Reimport(std::string(assetPath), error))
        {
            return Fail(error.empty() ? "Reimport failed." : error);
        }
        return OkResult();
    }

    std::string AssetEditorServices::NormalizeExtensionToken(std::string_view sourcePathOrExt)
    {
        std::string token(sourcePathOrExt);
        for (char& ch : token)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (token.empty())
        {
            return token;
        }
        if (token.find('.') == std::string::npos)
        {
            return "." + token;
        }
        if (token.front() != '.')
        {
            const std::filesystem::path asPath(token);
            return asPath.extension().string();
        }
        // bare extension like ".fbx"
        if (token.find('/') == std::string::npos && token.find('\\') == std::string::npos
            && token.find('.') == 0 && token.find('.', 1) == std::string::npos)
        {
            return token;
        }
        return std::filesystem::path(token).extension().string();
    }

    void AssetEditorServices::FillProductInfo(
        const ImportProductDescriptor& product,
        AssetImportProductInfo& outInfo)
    {
        outInfo.ProductId = product.ProductId;
        outInfo.DisplayName = product.DisplayName;
        outInfo.RequiredOptions.clear();
        outInfo.OptionalOptions.clear();
        outInfo.Notes.clear();

        if (product.ProductId == "SkeletalMesh")
        {
            outInfo.Notes = "Creates buddy Skeleton asset alongside the mesh.";
        }
        if (product.bNeedsSkeletonPicker || product.ProductId == "AnimationClip")
        {
            AssetImportOptionDescriptor option;
            option.Key = "skeletonAssetPath";
            option.Type = "string";
            option.Meaning = "Project-relative Skeleton asset path required for AnimationClip.";
            outInfo.RequiredOptions.push_back(std::move(option));

            AssetImportOptionDescriptor animIndex;
            animIndex.Key = "animationIndex";
            animIndex.Type = "int";
            animIndex.Meaning = "Clip index inside the source file (default 0).";
            outInfo.OptionalOptions.push_back(std::move(animIndex));
        }
    }

    int AssetEditorServices::ImportDependencyOrder(std::string_view productId)
    {
        if (productId == "SkeletalMesh")
        {
            return 10;
        }
        if (productId == "StaticMesh" || productId == "NativeCopy")
        {
            return 20;
        }
        if (productId == "AnimationClip")
        {
            return 30;
        }
        return 50;
    }

    AssetEditorResult AssetEditorServices::ValidateProductOptions(
        const AssetImportProductSpec& spec,
        bool skeletalMeshInBatch)
    {
        if (spec.ProductId.empty())
        {
            return Fail("productId is required.");
        }

        const ImportProductDescriptor* product =
            AssetManager::Get().FindImportProduct(spec.ProductId);
        if (product == nullptr)
        {
            return Fail("Unknown productId '" + spec.ProductId + "'.");
        }

        for (const auto& [key, value] : spec.Options)
        {
            (void)value;
            const bool known = key == "skeletonAssetPath" || key == "animationIndex"
                || key == "channelSemantics" || key == "sRGB" || key == "wrapU" || key == "wrapV"
                || key == "filter";
            if (!known)
            {
                return Fail("Unknown import option '" + key + "' for product '" + spec.ProductId + "'.");
            }
        }

        if (spec.ProductId == "Texture2D")
        {
            const auto semanticsIt = spec.Options.find("channelSemantics");
            if (semanticsIt != spec.Options.end() && !semanticsIt->second.empty())
            {
                const std::string& semantics = semanticsIt->second;
                if (semantics != "Color" && semantics != "SingleChannel" && semantics != "Mask")
                {
                    return Fail(
                        "options.channelSemantics must be Color, SingleChannel, or Mask.");
                }
            }
        }

        if (product->bNeedsSkeletonPicker || product->ProductId == "AnimationClip")
        {
            const auto skeletonIt = spec.Options.find("skeletonAssetPath");
            const bool hasSkeleton = skeletonIt != spec.Options.end() && !skeletonIt->second.empty();
            if (!hasSkeleton && !skeletalMeshInBatch)
            {
                return Fail("AnimationClip requires options.skeletonAssetPath (or include SkeletalMesh in the same products batch).");
            }
        }
        return OkResult();
    }

    bool AssetEditorServices::TryResolveSourceAbsolutePath(
        std::string_view sourcePath,
        std::filesystem::path& outAbsolute,
        std::string& outError)
    {
        outError.clear();
        if (sourcePath.empty())
        {
            outError = "sourcePath is required.";
            return false;
        }

        const std::filesystem::path input{std::string(sourcePath)};
        std::error_code errorCode;
        if (input.is_absolute())
        {
            outAbsolute = std::filesystem::weakly_canonical(input, errorCode);
        }
        else
        {
            outAbsolute = std::filesystem::weakly_canonical(std::filesystem::absolute(input), errorCode);
        }
        if (errorCode || !std::filesystem::exists(outAbsolute) || !std::filesystem::is_regular_file(outAbsolute))
        {
            outError = "sourcePath does not exist or is not a file: " + std::string(sourcePath);
            return false;
        }
        return true;
    }

    AssetEditorResult AssetEditorServices::ListImportProducts(
        std::string_view sourcePathOrExt,
        std::vector<AssetImportProductInfo>& outProducts) const
    {
        outProducts.clear();
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }
        if (sourcePathOrExt.empty())
        {
            return Fail("sourcePath or extension is required.");
        }

        const std::string extension = NormalizeExtensionToken(sourcePathOrExt);
        if (extension.empty())
        {
            return Fail("Could not resolve a source extension.");
        }

        for (const ImportProductDescriptor& product : AssetManager::Get().GetImportProducts())
        {
            if (product.AcceptsSourceExtension == nullptr || !product.AcceptsSourceExtension(extension))
            {
                continue;
            }
            AssetImportProductInfo info;
            FillProductInfo(product, info);
            outProducts.push_back(std::move(info));
        }
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::Import(
        std::string_view sourcePath,
        std::string_view destDirectory,
        const std::vector<AssetImportProductSpec>& products,
        bool overwrite,
        std::vector<AssetImportCreatedInfo>& outCreated,
        std::vector<std::string>& outWarnings)
    {
        outCreated.clear();
        outWarnings.clear();
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }
        if (destDirectory.empty())
        {
            return Fail("destDirectory is required.");
        }

        std::filesystem::path sourceAbsolute;
        std::string resolveError;
        if (!TryResolveSourceAbsolutePath(sourcePath, sourceAbsolute, resolveError))
        {
            return Fail(resolveError);
        }

        const std::string extension = sourceAbsolute.extension().string();
        std::vector<const ImportProductDescriptor*> compatible;
        for (const ImportProductDescriptor& product : AssetManager::Get().GetImportProducts())
        {
            if (product.AcceptsSourceExtension != nullptr && product.AcceptsSourceExtension(extension))
            {
                compatible.push_back(&product);
            }
        }
        if (compatible.empty())
        {
            return Fail("No import product accepts extension '" + extension + "'.");
        }

        std::vector<AssetImportProductSpec> resolved = products;
        if (resolved.empty())
        {
            if (compatible.size() == 1 && !compatible[0]->bNeedsSkeletonPicker)
            {
                AssetImportProductSpec spec;
                spec.ProductId = compatible[0]->ProductId;
                resolved.push_back(std::move(spec));
            }
            else
            {
                std::string message = "Multiple import products match; specify products[]. Candidates:";
                for (const ImportProductDescriptor* product : compatible)
                {
                    message += " ";
                    message += product->ProductId;
                }
                return Fail(std::move(message));
            }
        }

        bool skeletalMeshInBatch = false;
        for (const AssetImportProductSpec& spec : resolved)
        {
            if (spec.ProductId == "SkeletalMesh")
            {
                skeletalMeshInBatch = true;
                break;
            }
        }

        for (const AssetImportProductSpec& spec : resolved)
        {
            const AssetEditorResult validate = ValidateProductOptions(spec, skeletalMeshInBatch);
            if (!validate.Ok)
            {
                return validate;
            }

            bool accepted = false;
            for (const ImportProductDescriptor* product : compatible)
            {
                if (product->ProductId == spec.ProductId)
                {
                    accepted = true;
                    break;
                }
            }
            if (!accepted)
            {
                return Fail("productId '" + spec.ProductId + "' is incompatible with source extension '" + extension + "'.");
            }
        }

        std::sort(
            resolved.begin(),
            resolved.end(),
            [](const AssetImportProductSpec& left, const AssetImportProductSpec& right)
            {
                return ImportDependencyOrder(left.ProductId) < ImportDependencyOrder(right.ProductId);
            });

        std::string inferredSkeletonPath;
        const PathRegistry& paths = PathRegistry::Get();
        std::filesystem::path destAbs = paths.GetProjectContentRoot() / std::filesystem::path(std::string(destDirectory));

        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
        for (const AssetImportProductSpec& spec : resolved)
        {
            ImportRequest request;
            request.SourcePath = sourceAbsolute;
            request.DestDirectory = destAbs;
            request.ProductId = spec.ProductId;
            request.bOverwriteExisting = overwrite;

            const auto skeletonIt = spec.Options.find("skeletonAssetPath");
            if (skeletonIt != spec.Options.end() && !skeletonIt->second.empty())
            {
                request.SkeletonAssetPath = skeletonIt->second;
            }
            else if (spec.ProductId == "AnimationClip" && !inferredSkeletonPath.empty())
            {
                request.SkeletonAssetPath = inferredSkeletonPath;
            }

            const auto animIt = spec.Options.find("animationIndex");
            if (animIt != spec.Options.end() && !animIt->second.empty())
            {
                try
                {
                    request.AnimationIndex = std::stoi(animIt->second);
                }
                catch (...)
                {
                    return Fail("options.animationIndex must be an integer.");
                }
            }

            const auto semanticsIt = spec.Options.find("channelSemantics");
            if (semanticsIt != spec.Options.end() && !semanticsIt->second.empty())
            {
                request.ChannelSemantics = semanticsIt->second;
            }

            const ImportResult importResult = AssetManager::Get().Import(request);
            if (!importResult.bSuccess)
            {
                return Fail(importResult.ErrorMessage.empty() ? "Import failed." : importResult.ErrorMessage);
            }

            for (const std::string& warning : importResult.Warnings)
            {
                outWarnings.push_back(warning);
            }
            for (const ImportCreatedAsset& created : importResult.Created)
            {
                AssetImportCreatedInfo info;
                info.AssetPath = created.AssetPath;
                info.AssetTypeId = created.AssetTypeId;
                info.Guid = created.Guid.ToString();
                outCreated.push_back(std::move(info));

                if (created.AssetTypeId == "Skeleton")
                {
                    inferredSkeletonPath = created.AssetPath;
                }
                else if (created.AssetTypeId == "SkeletalMesh")
                {
                    // Buddy skeleton convention: stem_Skeleton.meskeleton next to mesh.
                    const std::filesystem::path meshPath(created.AssetPath);
                    inferredSkeletonPath =
                        (meshPath.parent_path() / (meshPath.stem().string() + "_Skeleton.meskeleton"))
                            .lexically_normal()
                            .generic_string();
                }
            }
        }

        return OkResult();
    }

    AssetEditorResult AssetEditorServices::CreateFolder(
        std::string_view parentDirectory,
        std::string_view folderName,
        std::string& outCreatedDirectoryRel)
    {
        outCreatedDirectoryRel.clear();
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }
        std::string error;
        std::string created;
        if (!AssetManager::Get().CreateFolder(std::string(parentDirectory), std::string(folderName), error, &created))
        {
            return Fail(error.empty() ? "CreateFolder failed." : error);
        }
        outCreatedDirectoryRel = std::move(created);
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::Move(std::string_view oldPath, std::string_view newPath)
    {
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }
        std::string error;
        if (!AssetManager::Get().MoveAsset(std::string(oldPath), std::string(newPath), error))
        {
            return Fail(error.empty() ? "MoveAsset failed." : error);
        }
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::Rename(std::string_view oldPath, std::string_view newFileName)
    {
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }
        std::string error;
        if (!AssetManager::Get().RenameAsset(std::string(oldPath), std::string(newFileName), error))
        {
            return Fail(error.empty() ? "RenameAsset failed." : error);
        }
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::RenameFolder(
        std::string_view directoryRel,
        std::string_view newFolderName)
    {
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }
        std::string error;
        if (!AssetManager::Get().RenameFolder(std::string(directoryRel), std::string(newFolderName), error))
        {
            return Fail(error.empty() ? "RenameFolder failed." : error);
        }
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::DeleteFolder(std::string_view directoryRel)
    {
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }
        std::string error;
        if (!AssetManager::Get().DeleteFolder(std::string(directoryRel), error, true))
        {
            return Fail(error.empty() ? "DeleteFolder failed." : error);
        }
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::BatchRename(
        const std::vector<std::string>& orderedPaths,
        std::string_view baseStem)
    {
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }
        if (orderedPaths.empty())
        {
            return Fail("No assets to rename.");
        }
        if (baseStem.empty())
        {
            return Fail("baseStem is required.");
        }
        const std::string stem(baseStem);
        if (stem.find('/') != std::string::npos || stem.find('\\') != std::string::npos)
        {
            return Fail("baseStem must not contain path separators.");
        }

        AssetManager& assets = AssetManager::Get();
        std::vector<std::pair<std::string, std::string>> planned;
        planned.reserve(orderedPaths.size());
        for (std::size_t index = 0; index < orderedPaths.size(); ++index)
        {
            const std::string& oldPath = orderedPaths[index];
            const AssetMeta* meta = assets.FindAssetMetaByPath(oldPath);
            if (meta == nullptr)
            {
                return Fail("Asset not found: " + oldPath);
            }
            const std::filesystem::path oldFs{oldPath};
            const std::string extension = oldFs.extension().string();
            std::string newFileName = stem;
            if (index > 0)
            {
                newFileName += "_";
                newFileName += std::to_string(index);
            }
            newFileName += extension;
            const std::string newPath =
                (oldFs.parent_path() / newFileName).lexically_normal().generic_string();
            if (assets.FindAssetMetaByPath(newPath) != nullptr && newPath != oldPath)
            {
                return Fail("Rename conflict: " + newPath);
            }
            for (const auto& prior : planned)
            {
                if (prior.second == newPath)
                {
                    return Fail("Rename conflict within batch: " + newPath);
                }
            }
            planned.emplace_back(oldPath, newFileName);
        }

        for (const auto& item : planned)
        {
            std::string error;
            if (!assets.RenameAsset(item.first, item.second, error))
            {
                return Fail(error.empty() ? "RenameAsset failed." : error);
            }
        }
        return OkResult();
    }


}
