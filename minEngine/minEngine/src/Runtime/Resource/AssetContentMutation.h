#pragma once

#include <string>

namespace minEngine
{
    class AssetManager;

    /**
     * Content workspace mutations: disk + .meta + AssetRegistry + load-cache invalidation.
     * Not a generic filesystem facade. AssetManager keeps same-named public forwards.
     */
    class AssetContentMutation
    {
    public:
        explicit AssetContentMutation(AssetManager& manager);

        bool CreateFolder(
            const std::string& parentDirectoryRel,
            const std::string& folderName,
            std::string& outError,
            std::string* outCreatedDirectoryRel = nullptr);

        bool RenameFolder(
            const std::string& directoryRel,
            const std::string& newFolderName,
            std::string& outError);

        bool DeleteFolder(
            const std::string& directoryRel,
            std::string& outError,
            bool requireEmpty = true);

        bool MoveAsset(const std::string& oldPath, const std::string& newPath, std::string& outError);

        bool RenameAsset(
            const std::string& oldPath,
            const std::string& newFileName,
            std::string& outError);

        bool DeleteAsset(const std::string& assetPath, std::string& outError);

        bool DeleteAsset(
            const std::string& assetPath,
            std::string& outError,
            bool bUnpackOpenPrefabInstanceRefs);

    private:
        AssetManager& m_Manager;
    };
}
