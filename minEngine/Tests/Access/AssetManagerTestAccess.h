#pragma once

#include "Runtime/Resource/AssetManager.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace minEngine::Testing
{
    /** Snapshot of what project-relative path normalisation actually did. */
    struct AssetPathNormalizationStats
    {
        uint64_t InputCanonicalizations = 0;
        uint64_t RootRefreshes = 0;
        uint64_t CacheHits = 0;
        uint64_t CacheMisses = 0;
        uint64_t CachedEntries = 0;
    };

    template<>
    class TestAccess<AssetManager>
    {
    public:
        static void SetInstance(AssetManager* instance)
        {
            AssetManager::SetInstance(instance);
        }

        static std::string NormalizeProjectRelativeAssetPath(
            const AssetManager& manager,
            const std::string& path)
        {
            return manager.NormalizeProjectRelativeAssetPath(path);
        }

        static AssetPathNormalizationStats GetNormalizationStats(const AssetManager& manager)
        {
            std::lock_guard<std::mutex> lock(manager.m_PathCache.Mutex);

            AssetPathNormalizationStats stats;
            stats.InputCanonicalizations = manager.m_PathCache.InputCanonicalizations;
            stats.RootRefreshes = manager.m_PathCache.RootRefreshes;
            stats.CacheHits = manager.m_PathCache.Hits;
            stats.CacheMisses = manager.m_PathCache.Misses;
            stats.CachedEntries = manager.m_PathCache.Results.size();
            return stats;
        }
    };
}
