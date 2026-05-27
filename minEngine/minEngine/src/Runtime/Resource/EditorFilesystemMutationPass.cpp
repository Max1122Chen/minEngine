#include "Runtime/Resource/EditorFilesystemMutationPass.h"

#include "Runtime/Resource/AssetManager.h"

#include <algorithm>

namespace minEngine
{
    std::mutex EditorFilesystemMutationPass::s_Mutex;
    std::unordered_map<std::string, EditorFilesystemMutationPass::TimePoint>
        EditorFilesystemMutationPass::s_MutatedPathsUntil;

    EditorFilesystemMutationPass::Scope::Scope()
    {
        m_Active = true;
    }

    EditorFilesystemMutationPass::Scope::~Scope()
    {
        (void)m_Active;
    }

    std::string EditorFilesystemMutationPass::CanonicalizePathKey(const std::filesystem::path& absolutePath)
    {
        if (absolutePath.empty())
        {
            return std::string();
        }

        std::error_code errorCode;
        const std::filesystem::path canonicalPath =
            std::filesystem::weakly_canonical(absolutePath, errorCode);
        if (errorCode)
        {
            return absolutePath.lexically_normal().generic_string();
        }

        return canonicalPath.generic_string();
    }

    void EditorFilesystemMutationPass::PruneExpired()
    {
        const TimePoint now = Clock::now();
        for (auto iter = s_MutatedPathsUntil.begin(); iter != s_MutatedPathsUntil.end();)
        {
            if (iter->second <= now)
            {
                iter = s_MutatedPathsUntil.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    bool EditorFilesystemMutationPass::IsPathRegistered(const std::string& canonicalKey)
    {
        if (canonicalKey.empty())
        {
            return false;
        }

        const auto iter = s_MutatedPathsUntil.find(canonicalKey);
        if (iter == s_MutatedPathsUntil.end())
        {
            return false;
        }

        if (iter->second <= Clock::now())
        {
            s_MutatedPathsUntil.erase(iter);
            return false;
        }

        return true;
    }

    void EditorFilesystemMutationPass::RegisterPathAndParents(const std::filesystem::path& absolutePath)
    {
        const std::string canonicalKey = CanonicalizePathKey(absolutePath);
        if (canonicalKey.empty())
        {
            return;
        }

        const TimePoint expiresAt = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                                         std::chrono::duration<float>(kDefaultTtlSeconds));

        std::filesystem::path path = std::filesystem::path(canonicalKey);
        while (true)
        {
            const std::string key = path.generic_string();
            if (key.empty() || key == ".")
            {
                break;
            }

            auto existingIter = s_MutatedPathsUntil.find(key);
            if (existingIter == s_MutatedPathsUntil.end() || existingIter->second < expiresAt)
            {
                s_MutatedPathsUntil[key] = expiresAt;
            }

            if (!path.has_parent_path())
            {
                break;
            }

            const std::filesystem::path parentPath = path.parent_path();
            if (parentPath == path)
            {
                break;
            }

            path = parentPath;
        }
    }

    void EditorFilesystemMutationPass::NoteMutatedAbsolutePath(const std::filesystem::path& absolutePath)
    {
        if (absolutePath.empty())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(s_Mutex);
        PruneExpired();
        RegisterPathAndParents(absolutePath);
    }

    void EditorFilesystemMutationPass::NoteMutatedProjectRelativePath(std::string_view projectRelativePath)
    {
        if (projectRelativePath.empty() || !AssetManager::HasInstance())
        {
            return;
        }

        NoteMutatedAbsolutePath(AssetManager::Get().ResolveAssetAbsolutePath(projectRelativePath));
    }

    bool EditorFilesystemMutationPass::ShouldIgnoreWatcherEvent(const std::filesystem::path& absolutePath)
    {
        if (absolutePath.empty())
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(s_Mutex);
        PruneExpired();

        std::filesystem::path path = std::filesystem::path(CanonicalizePathKey(absolutePath));
        while (true)
        {
            const std::string key = path.generic_string();
            if (!key.empty() && key != "." && IsPathRegistered(key))
            {
                return true;
            }

            if (!path.has_parent_path())
            {
                break;
            }

            const std::filesystem::path parentPath = path.parent_path();
            if (parentPath == path)
            {
                break;
            }

            path = parentPath;
        }

        return false;
    }

    void EditorFilesystemMutationPass::Clear()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_MutatedPathsUntil.clear();
    }
}
