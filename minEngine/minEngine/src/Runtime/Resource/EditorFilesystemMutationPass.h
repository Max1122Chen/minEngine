#pragma once

#include "Core.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace minEngine
{
    // Marks disk paths touched by Editor-initiated AssetManager CRUD so ProjectAssetWatcher
    // can ignore duplicate efsw notifications (registry is already updated by CRUD).
    class EditorFilesystemMutationPass
    {
    public:
        class Scope
        {
        public:
            Scope();
            ~Scope();

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;

        private:
            bool m_Active = false;
        };

        static constexpr float kDefaultTtlSeconds = 1.0f;

        static void NoteMutatedAbsolutePath(const std::filesystem::path& absolutePath);
        static void NoteMutatedProjectRelativePath(std::string_view projectRelativePath);
        static bool ShouldIgnoreWatcherEvent(const std::filesystem::path& absolutePath);

        static void Clear();

    private:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        static std::string CanonicalizePathKey(const std::filesystem::path& absolutePath);
        static void RegisterPathAndParents(const std::filesystem::path& absolutePath);
        static void PruneExpired();
        static bool IsPathRegistered(const std::string& canonicalKey);

        static std::mutex s_Mutex;
        static std::unordered_map<std::string, TimePoint> s_MutatedPathsUntil;
    };
}
