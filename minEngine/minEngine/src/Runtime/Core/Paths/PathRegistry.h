#pragma once

#include "Core.h"

#include <filesystem>
#include <optional>
#include <string>

namespace minEngine
{
    struct EngineConfig;

    /** Resolved engine/project paths (M0). Single source of truth after bootstrap. */
    class PathRegistry
    {
    public:
        static PathRegistry& Get();

        /** Discover EngineConfig.meconfig, load JSON, resolve EngineRoot + EngineDefaultAssets. */
        bool LoadEngineConfiguration(int argc, char** argv, EngineConfig& outConfig);

        void SetEngineDefaultAssetsRootOverride(std::filesystem::path absolutePath);
        void ClearEngineDefaultAssetsRootOverride();

        void SetProjectRoots(const std::filesystem::path& projectRoot);

        void ClearProjectRoots();

        const std::filesystem::path& GetEngineRoot() const { return m_EngineRoot; }
        const std::filesystem::path& GetEngineConfigFilePath() const { return m_EngineConfigFilePath; }
        const std::filesystem::path& GetEngineDefaultAssetsRoot() const { return m_EngineDefaultAssetsRoot; }
        std::string GetEngineDefaultAssetsRootString() const;

        const std::filesystem::path& GetProjectRoot() const { return m_ProjectRoot; }
        const std::filesystem::path& GetProjectContentRoot() const { return m_ProjectContentRoot; }

        std::filesystem::path ResolveEngineRelative(const std::filesystem::path& relativePath) const;
        std::filesystem::path ResolveProjectRelative(const std::filesystem::path& relativePath) const;

        /** Config value: relative → baseRoot; absolute → normalize as-is (legacy). */
        static std::filesystem::path ResolvePathAgainstRoot(
            const std::filesystem::path& baseRoot,
            const std::filesystem::path& pathInConfig,
            const std::filesystem::path& defaultRelativeIfEmpty = "Assets/EngineDefault");

        static std::optional<std::filesystem::path> DiscoverEngineConfigFile(int argc, char** argv);

        void LogResolvedPaths() const;

    private:
        static PathRegistry* s_Instance;

        bool ApplyEngineConfig(
            const std::filesystem::path& configFilePath,
            const EngineConfig& config,
            const std::optional<std::filesystem::path>& engineRootOverride);

        std::filesystem::path m_EngineRoot;
        std::filesystem::path m_EngineConfigFilePath;
        std::filesystem::path m_EngineDefaultAssetsRoot;
        bool m_HasEngineDefaultAssetsOverride = false;

        std::filesystem::path m_ProjectRoot;
        std::filesystem::path m_ProjectContentRoot;
    };
}
