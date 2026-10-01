#include "AssetContentMutation.h"

#include "AssetManager.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include <filesystem>
#include <system_error>
#include <vector>

namespace minEngine
{
    AssetContentMutation::AssetContentMutation(AssetManager& manager)
        : m_Manager(manager)
    {
    }

    bool AssetContentMutation::DeleteAsset(const std::string& assetPath, std::string& outError)
    {
        return DeleteAsset(assetPath, outError, false);
    }

    bool AssetContentMutation::DeleteAsset(
        const std::string& assetPath,
        std::string& outError,
        bool bUnpackOpenPrefabInstanceRefs)
    {
        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
        outError.clear();

        const std::string projectRelative = m_Manager.NormalizeProjectRelativeAssetPath(assetPath);
        if (projectRelative.empty())
        {
            outError = "invalid or out-of-project path";
            return false;
        }

        const AssetMeta* metaPtr = m_Manager.FindAssetMetaByPath(projectRelative);
        if (metaPtr == nullptr)
        {
            outError = "asset not registered";
            return false;
        }

        const AssetMeta meta = *metaPtr;

        if (meta.AssetType == "Prefab")
        {
            const std::vector<PrefabInstanceRef> refs =
                PrefabUtility::FindInstanceRefsInEditorScene(meta.Guid);
            if (!refs.empty())
            {
                if (!bUnpackOpenPrefabInstanceRefs)
                {
                    outError = PrefabUtility::FormatPrefabInstanceRefsMessage(projectRelative, refs);
                    return false;
                }

                for (const PrefabInstanceRef& ref : refs)
                {
                    if (ref.Scene == nullptr)
                    {
                        continue;
                    }

                    std::string unpackError;
                    if (!PrefabUtility::UnpackInstance(*ref.Scene, ref.RootInstanceGuid, &unpackError))
                    {
                        outError = unpackError.empty()
                            ? "Failed to unpack Prefab instance before delete."
                            : unpackError;
                        return false;
                    }
                }
            }
        }
        else
        {
            m_Manager.LogReferenceWarningsForDelete(meta);
        }

        const std::filesystem::path absolutePath = m_Manager.ResolveAssetAbsolutePath(projectRelative);
        const std::filesystem::path metaAbsolutePath = m_Manager.BuildMetaAbsolutePath(projectRelative);

        m_Manager.NoteEditorFilesystemMutation(absolutePath);
        m_Manager.NoteEditorFilesystemMutation(metaAbsolutePath);

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

        m_Manager.EvictLoadedAssetCache(projectRelative);
        m_Manager.UncacheMeta(projectRelative);

        if (meta.AssetType == "Scene" && SceneManager::HasInstance())
        {
            SceneManager::Get().UnregisterScene(meta.AssetName);
        }

        return true;
    }

    bool AssetContentMutation::MoveAsset(
        const std::string& oldPath,
        const std::string& newPath,
        std::string& outError)
    {
        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
        outError.clear();

        const std::string oldRel = m_Manager.NormalizeProjectRelativeAssetPath(oldPath);
        const std::string newRel = m_Manager.NormalizeProjectRelativeAssetPath(newPath);
        if (oldRel.empty() || newRel.empty())
        {
            outError = "invalid or out-of-project path";
            return false;
        }

        if (oldRel == newRel)
        {
            return true;
        }

        const AssetMeta* oldMetaPtr = m_Manager.FindAssetMetaByPath(oldRel);
        if (oldMetaPtr == nullptr)
        {
            outError = "asset not registered";
            return false;
        }

        if (m_Manager.FindAssetMetaByPath(newRel) != nullptr)
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

        const std::filesystem::path absoluteOld = m_Manager.ResolveAssetAbsolutePath(oldRel);
        const std::filesystem::path absoluteNew = m_Manager.ResolveAssetAbsolutePath(newRel);
        const std::filesystem::path metaAbsoluteOld = m_Manager.BuildMetaAbsolutePath(oldRel);
        const std::filesystem::path metaAbsoluteNew = m_Manager.BuildMetaAbsolutePath(newRel);

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

        m_Manager.NoteEditorFilesystemMutation(absoluteOld);
        m_Manager.NoteEditorFilesystemMutation(absoluteNew);
        m_Manager.NoteEditorFilesystemMutation(metaAbsoluteOld);
        m_Manager.NoteEditorFilesystemMutation(metaAbsoluteNew);
        m_Manager.NoteEditorFilesystemMutation(newParent);

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
        if (!m_Manager.MoveRegistryEntry(oldRel, newRel, updatedMeta))
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

        m_Manager.MoveLoadedAssetCacheKey(oldRel, newRel);

        if (!m_Manager.WriteMetaFile(updatedMeta))
        {
            ME_LOG(LogAsset, Warn, "MoveAsset: meta file write failed after move to '{}'", newRel);
        }

        return true;
    }

    bool AssetContentMutation::RenameAsset(
        const std::string& oldPath,
        const std::string& newFileName,
        std::string& outError)
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

        const std::string oldRel = m_Manager.NormalizeProjectRelativeAssetPath(oldPath);
        if (oldRel.empty())
        {
            outError = "invalid or out-of-project path";
            return false;
        }

        const std::filesystem::path newRelPath =
            std::filesystem::path(oldRel).parent_path() / newFileName;
        return MoveAsset(oldRel, newRelPath.lexically_normal().generic_string(), outError);
    }

    bool AssetContentMutation::CreateFolder(
        const std::string& parentDirectoryRel,
        const std::string& folderName,
        std::string& outError,
        std::string* outCreatedDirectoryRel)
    {
        outError.clear();
        if (outCreatedDirectoryRel != nullptr)
        {
            outCreatedDirectoryRel->clear();
        }

        if (folderName.empty()
            || folderName.find('/') != std::string::npos
            || folderName.find('\\') != std::string::npos
            || folderName == "."
            || folderName == "..")
        {
            outError = "invalid folder name";
            return false;
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        if (contentRoot.empty())
        {
            outError = "ProjectContentRoot is not set";
            return false;
        }

        std::filesystem::path parentAbs = contentRoot;
        if (!parentDirectoryRel.empty())
        {
            parentAbs = contentRoot / std::filesystem::path(parentDirectoryRel);
        }

        const std::filesystem::path createdAbs = parentAbs / folderName;
        if (std::filesystem::exists(createdAbs))
        {
            outError = "folder already exists";
            return false;
        }

        m_Manager.NoteEditorFilesystemMutation(createdAbs);
        std::error_code createError;
        if (!std::filesystem::create_directories(createdAbs, createError) || createError)
        {
            outError = "failed to create folder: " + createError.message();
            return false;
        }

        if (outCreatedDirectoryRel != nullptr)
        {
            std::filesystem::path rel = parentDirectoryRel.empty()
                ? std::filesystem::path(folderName)
                : (std::filesystem::path(parentDirectoryRel) / folderName);
            *outCreatedDirectoryRel = rel.lexically_normal().generic_string();
        }
        return true;
    }

    bool AssetContentMutation::RenameFolder(
        const std::string& directoryRel,
        const std::string& newFolderName,
        std::string& outError)
    {
        outError.clear();
        if (directoryRel.empty())
        {
            outError = "cannot rename content root";
            return false;
        }
        if (newFolderName.empty()
            || newFolderName.find('/') != std::string::npos
            || newFolderName.find('\\') != std::string::npos)
        {
            outError = "invalid folder name";
            return false;
        }

        const std::vector<const AssetMeta*> under =
            m_Manager.FindAssetMetasUnderDirectory(directoryRel);
        if (!under.empty())
        {
            outError = "folder is not empty (contains registered assets); move assets first";
            return false;
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        const std::filesystem::path oldAbs = contentRoot / std::filesystem::path(directoryRel);
        if (!std::filesystem::exists(oldAbs) || !std::filesystem::is_directory(oldAbs))
        {
            outError = "folder does not exist";
            return false;
        }

        std::error_code iterError;
        const auto it = std::filesystem::directory_iterator(oldAbs, iterError);
        if (!iterError && it != std::filesystem::directory_iterator{})
        {
            outError = "folder is not empty on disk";
            return false;
        }

        const std::filesystem::path newAbs = oldAbs.parent_path() / newFolderName;
        if (std::filesystem::exists(newAbs))
        {
            outError = "destination folder already exists";
            return false;
        }

        m_Manager.NoteEditorFilesystemMutation(oldAbs);
        m_Manager.NoteEditorFilesystemMutation(newAbs);
        std::error_code renameError;
        std::filesystem::rename(oldAbs, newAbs, renameError);
        if (renameError)
        {
            outError = "failed to rename folder: " + renameError.message();
            return false;
        }
        return true;
    }

    bool AssetContentMutation::DeleteFolder(
        const std::string& directoryRel,
        std::string& outError,
        bool requireEmpty)
    {
        outError.clear();
        if (directoryRel.empty())
        {
            outError = "cannot delete content root";
            return false;
        }

        const std::vector<const AssetMeta*> under = m_Manager.FindAssetMetasUnderDirectory(directoryRel);
        if (!under.empty())
        {
            outError = "folder contains registered assets";
            return false;
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        const std::filesystem::path absPath = contentRoot / std::filesystem::path(directoryRel);
        if (!std::filesystem::exists(absPath))
        {
            return true;
        }
        if (!std::filesystem::is_directory(absPath))
        {
            outError = "path is not a directory";
            return false;
        }

        if (requireEmpty)
        {
            std::error_code iterError;
            const auto it = std::filesystem::directory_iterator(absPath, iterError);
            if (!iterError && it != std::filesystem::directory_iterator{})
            {
                outError = "folder is not empty";
                return false;
            }
        }

        m_Manager.NoteEditorFilesystemMutation(absPath);
        std::error_code removeError;
        if (requireEmpty)
        {
            if (!std::filesystem::remove(absPath, removeError) || removeError)
            {
                outError = "failed to delete folder: " + removeError.message();
                return false;
            }
        }
        else
        {
            const auto removed = std::filesystem::remove_all(absPath, removeError);
            if (removeError || removed == static_cast<std::uintmax_t>(-1))
            {
                outError = "failed to delete folder tree: " + removeError.message();
                return false;
            }
        }
        return true;
    }
}
