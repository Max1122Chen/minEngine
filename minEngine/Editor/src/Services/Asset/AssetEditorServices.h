#pragma once

#include "Runtime/Core/Serialization/Json.h"
#include "Runtime/Function/Render/RHI/RHISampler.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class AssetMeta;
    class IEditorContext;

    struct AssetEditorResult
    {
        bool Ok = false;
        std::string Message;
    };

    struct AssetImportOptionDescriptor
    {
        std::string Key;
        std::string Type;
        std::string Meaning;
    };

    struct AssetImportProductInfo
    {
        std::string ProductId;
        std::string DisplayName;
        std::vector<AssetImportOptionDescriptor> RequiredOptions;
        std::vector<AssetImportOptionDescriptor> OptionalOptions;
        std::string Notes;
    };

    struct AssetImportProductSpec
    {
        std::string ProductId;
        std::unordered_map<std::string, std::string> Options;
    };

    struct AssetImportCreatedInfo
    {
        std::string AssetPath;
        std::string AssetTypeId;
        std::string Guid;
    };

    /** Asset library primitives. Create/delete are not Scene undo units. */
    class AssetEditorServices
    {
    public:
        AssetEditorResult List(
            std::string_view directory,
            std::string_view assetType,
            std::vector<const AssetMeta*>& outMetas) const;

        /**
         * Create a creatable asset type. For DataAsset, optional runtimeClassName selects the
         * reflected instance class (empty = base DataAsset). Other asset types reject a non-empty
         * runtimeClassName.
         */
        AssetEditorResult Create(
            std::string_view assetType,
            std::string_view directory,
            std::string_view name,
            std::string& outPath,
            std::string& outGuid,
            std::string_view runtimeClassName = {});

        AssetEditorResult Delete(
            IEditorContext& context,
            std::string_view assetPath,
            bool unpackOpenPrefabInstanceRefs = false);

        AssetEditorResult Reimport(std::string_view assetPath);

        /** Discover compatible import products for a source path or extension (".fbx" / "png"). */
        AssetEditorResult ListImportProducts(
            std::string_view sourcePathOrExt,
            std::vector<AssetImportProductInfo>& outProducts) const;

        /**
         * Unified import facade. Empty products auto-picks when exactly one compatible product
         * does not require a skeleton picker; multiple candidates fail with a listed message.
         * Products run in dependency order (SkeletalMesh before AnimationClip).
         */
        AssetEditorResult Import(
            std::string_view sourcePath,
            std::string_view destDirectory,
            const std::vector<AssetImportProductSpec>& products,
            bool overwrite,
            std::vector<AssetImportCreatedInfo>& outCreated,
            std::vector<std::string>& outWarnings);

        AssetEditorResult CreateFolder(
            std::string_view parentDirectory,
            std::string_view folderName,
            std::string& outCreatedDirectoryRel);

        AssetEditorResult Move(std::string_view oldPath, std::string_view newPath);
        AssetEditorResult Rename(std::string_view oldPath, std::string_view newFileName);
        AssetEditorResult RenameFolder(std::string_view directoryRel, std::string_view newFolderName);
        AssetEditorResult DeleteFolder(std::string_view directoryRel);
        AssetEditorResult BatchRename(
            const std::vector<std::string>& orderedPaths,
            std::string_view baseStem);

        /**
         * Load a DataAsset (any reflected subclass) by path or guid, write one property from JSON,
         * then SaveAsset while the instance is still held. Empty path uses guid; both empty fails.
         */
        AssetEditorResult SetDataAssetProperty(
            std::string_view path,
            std::string_view guid,
            std::string_view propertyPath,
            const Json& value,
            std::string& outPath,
            std::string& outGuid,
            std::string& outRuntimeClassName);

        /**
         * Load Texture2D, set sampler axes (callers pass current values for unchanged axes),
         * then SaveAsset so .metex header matches the live fields.
         */
        AssetEditorResult SetTextureSampler(
            std::string_view path,
            RHISamplerFilter filter,
            RHISamplerAddress address);

    private:
        static AssetEditorResult OkResult();
        static AssetEditorResult Fail(std::string message);
        static bool IsCreatableAssetTypeId(std::string_view assetTypeId);

        static std::string NormalizeExtensionToken(std::string_view sourcePathOrExt);
        static void FillProductInfo(const class ImportProductDescriptor& product, AssetImportProductInfo& outInfo);
        static int ImportDependencyOrder(std::string_view productId);
        static AssetEditorResult ValidateProductOptions(
            const AssetImportProductSpec& spec,
            bool skeletalMeshInBatch);
        static bool TryResolveSourceAbsolutePath(
            std::string_view sourcePath,
            std::filesystem::path& outAbsolute,
            std::string& outError);
    };
}
