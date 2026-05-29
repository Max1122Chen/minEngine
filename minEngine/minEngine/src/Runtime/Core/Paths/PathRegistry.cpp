#include "PathRegistry.h"

#include "Runtime/Core/CLI/CommandLineResult.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/EngineConfig.h"

#include <cstdlib>
#include <vector>

namespace minEngine
{
    namespace
    {
        constexpr const char* kEngineConfigBaseName = "EngineConfig";
        constexpr const char* kEngineConfigExtension = ".meconfig";
        constexpr const char* kArgEngineConfigPrefix = "--engine-config=";
        constexpr const char* kArgEngineRootPrefix = "--engine-root=";
        constexpr int kMaxParentWalkDepth = 8;

        std::optional<std::filesystem::path> ParsePrefixedArg(int argc, char** argv, const char* prefix)
        {
            const std::string_view prefixView(prefix);
            for (int argIndex = 1; argIndex < argc; ++argIndex)
            {
                if (argv[argIndex] == nullptr)
                {
                    continue;
                }

                const std::string_view arg(argv[argIndex]);
                if (arg.size() > prefixView.size() && arg.substr(0, prefixView.size()) == prefixView)
                {
                    return std::filesystem::path(std::string(arg.substr(prefixView.size())));
                }
            }
            return std::nullopt;
        }

        std::optional<std::filesystem::path> PathFromEnvironment(const char* variableName)
        {
            if (variableName == nullptr)
            {
                return std::nullopt;
            }

            if (const char* value = std::getenv(variableName))
            {
                if (value[0] != '\0')
                {
                    return std::filesystem::path(value);
                }
            }
            return std::nullopt;
        }

        bool IsExistingConfigFile(const std::filesystem::path& candidate)
        {
            return !candidate.empty() && std::filesystem::exists(candidate) &&
                   std::filesystem::is_regular_file(candidate);
        }

        std::optional<std::filesystem::path> DiscoverEngineConfigByWalkAndEnvironment()
        {
            const std::filesystem::path cwd = std::filesystem::current_path();
            const std::filesystem::path cwdConfig =
                cwd / (std::string(kEngineConfigBaseName) + kEngineConfigExtension);
            if (IsExistingConfigFile(cwdConfig))
            {
                return cwdConfig;
            }

            std::filesystem::path walk = cwd;
            for (int depth = 0; depth < kMaxParentWalkDepth; ++depth)
            {
                const std::filesystem::path candidate =
                    walk / (std::string(kEngineConfigBaseName) + kEngineConfigExtension);
                if (IsExistingConfigFile(candidate))
                {
                    return candidate;
                }

                if (!walk.has_parent_path() || walk.parent_path() == walk)
                {
                    break;
                }
                walk = walk.parent_path();
            }

            if (const std::optional<std::filesystem::path> envConfig =
                    PathFromEnvironment("MINENGINE_ENGINE_CONFIG"))
            {
                if (IsExistingConfigFile(*envConfig))
                {
                    return envConfig;
                }
            }

            ME_CORE_ERROR(
                "PathRegistry: EngineConfig.meconfig not found (cwd='{}'). "
                "Set cwd to engine dist root, pass --engine-config=, or set MINENGINE_ENGINE_CONFIG.",
                cwd.string());
            return std::nullopt;
        }
    }

    PathRegistry* PathRegistry::s_Instance = nullptr;

    PathRegistry& PathRegistry::Get()
    {
        static PathRegistry instance;
        s_Instance = &instance;
        return instance;
    }

    std::filesystem::path PathRegistry::ResolvePathAgainstRoot(
        const std::filesystem::path& baseRoot,
        const std::filesystem::path& pathInConfig,
        const std::filesystem::path& defaultRelativeIfEmpty)
    {
        std::filesystem::path configured = pathInConfig;
        if (configured.empty())
        {
            configured = defaultRelativeIfEmpty;
        }

        if (configured.is_absolute())
        {
            return std::filesystem::weakly_canonical(configured);
        }

        return std::filesystem::weakly_canonical(baseRoot / configured);
    }

    std::optional<std::filesystem::path> PathRegistry::DiscoverEngineConfigFile(int argc, char** argv)
    {
        if (const std::optional<std::filesystem::path> explicitConfig =
                ParsePrefixedArg(argc, argv, kArgEngineConfigPrefix))
        {
            if (IsExistingConfigFile(*explicitConfig))
            {
                return explicitConfig;
            }

            ME_CORE_ERROR(
                "PathRegistry: --engine-config points to missing file '{}'.",
                explicitConfig->string());
            return std::nullopt;
        }

        return DiscoverEngineConfigByWalkAndEnvironment();
    }

    bool PathRegistry::ApplyEngineConfig(
        const std::filesystem::path& configFilePath,
        const EngineConfig& config,
        const std::optional<std::filesystem::path>& engineRootOverride)
    {
        m_EngineConfigFilePath = std::filesystem::weakly_canonical(configFilePath);
        m_EngineRoot = engineRootOverride.has_value()
                             ? std::filesystem::weakly_canonical(*engineRootOverride)
                             : m_EngineConfigFilePath.parent_path();

        if (!m_HasEngineDefaultAssetsOverride)
        {
            m_EngineDefaultAssetsRoot = ResolvePathAgainstRoot(
                m_EngineRoot,
                config.EngineDefaultAssetsRoot);
        }

        if (!std::filesystem::exists(m_EngineDefaultAssetsRoot) ||
            !std::filesystem::is_directory(m_EngineDefaultAssetsRoot))
        {
            ME_CORE_ERROR(
                "PathRegistry: EngineDefaultAssetsRoot does not exist: '{}'",
                m_EngineDefaultAssetsRoot.string());
            return false;
        }

        return true;
    }

    bool PathRegistry::LoadEngineConfiguration(const CommandLineResult& commandLine, EngineConfig& outConfig)
    {
        std::optional<std::filesystem::path> configPath;
        if (commandLine.EngineConfigPath.has_value())
        {
            if (IsExistingConfigFile(*commandLine.EngineConfigPath))
            {
                configPath = commandLine.EngineConfigPath;
            }
            else
            {
                ME_CORE_ERROR(
                    "PathRegistry: --engine-config points to missing file '{}'.",
                    commandLine.EngineConfigPath->string());
                return false;
            }
        }
        else
        {
            configPath = DiscoverEngineConfigByWalkAndEnvironment();
        }

        if (!configPath.has_value())
        {
            return false;
        }

        Serialization::JsonReaderArchive reader;
        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            configPath->string(),
            Reflection::GetClassName<EngineConfig>(),
            &outConfig,
            reader,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = true,
                .writeObjectTypeName = false,
                .allowObjectPtrSerialization = true,
            });
        if (!result.ok)
        {
            ME_CORE_ERROR(
                "PathRegistry: failed to load '{}' — {} (field: {})",
                configPath->string(),
                result.message,
                result.fieldPath);
            return false;
        }

        std::optional<std::filesystem::path> engineRootOverride = commandLine.EngineRootOverride;
        if (!engineRootOverride.has_value())
        {
            engineRootOverride = PathFromEnvironment("MINENGINE_ENGINE_ROOT");
        }

        if (!ApplyEngineConfig(*configPath, outConfig, engineRootOverride))
        {
            return false;
        }

        if (!outConfig.EngineDefaultAssetsRoot.empty() &&
            std::filesystem::path(outConfig.EngineDefaultAssetsRoot).is_absolute())
        {
            ME_CORE_WARN(
                "PathRegistry: EngineDefaultAssetsRoot is absolute in config; prefer relative to EngineRoot ('{}').",
                m_EngineRoot.string());
        }

        LogResolvedPaths();
        return true;
    }

    bool PathRegistry::LoadEngineConfiguration(int argc, char** argv, EngineConfig& outConfig)
    {
        CommandLineResult commandLine;
        if (const std::optional<std::filesystem::path> explicitConfig =
                ParsePrefixedArg(argc, argv, kArgEngineConfigPrefix))
        {
            commandLine.EngineConfigPath = explicitConfig;
        }

        if (const std::optional<std::filesystem::path> engineRoot =
                ParsePrefixedArg(argc, argv, kArgEngineRootPrefix))
        {
            commandLine.EngineRootOverride = engineRoot;
        }

        return LoadEngineConfiguration(commandLine, outConfig);
    }

    void PathRegistry::SetEngineDefaultAssetsRootOverride(std::filesystem::path absolutePath)
    {
        m_HasEngineDefaultAssetsOverride = true;
        m_EngineDefaultAssetsRoot = std::filesystem::weakly_canonical(std::move(absolutePath));
    }

    void PathRegistry::ClearEngineDefaultAssetsRootOverride()
    {
        m_HasEngineDefaultAssetsOverride = false;
    }

    void PathRegistry::SetProjectRoots(const std::filesystem::path& projectRoot)
    {
        if (projectRoot.empty())
        {
            ClearProjectRoots();
            return;
        }

        m_ProjectRoot = std::filesystem::weakly_canonical(projectRoot);
        m_ProjectContentRoot = m_ProjectRoot / "Assets";

        ME_CORE_INFO(
            "PathRegistry: ProjectRoot='{}' ProjectContent='{}'",
            m_ProjectRoot.string(),
            m_ProjectContentRoot.string());
    }

    void PathRegistry::ClearProjectRoots()
    {
        m_ProjectRoot.clear();
        m_ProjectContentRoot.clear();
    }

    std::string PathRegistry::GetEngineDefaultAssetsRootString() const
    {
        return m_EngineDefaultAssetsRoot.string();
    }

    std::filesystem::path PathRegistry::ResolveEngineRelative(
        const std::filesystem::path& relativePath) const
    {
        if (m_EngineRoot.empty())
        {
            return relativePath;
        }
        return ResolvePathAgainstRoot(m_EngineRoot, relativePath, relativePath);
    }

    std::filesystem::path PathRegistry::ResolveProjectRelative(
        const std::filesystem::path& relativePath) const
    {
        if (m_ProjectRoot.empty())
        {
            return relativePath;
        }
        return ResolvePathAgainstRoot(m_ProjectRoot, relativePath, relativePath);
    }

    void PathRegistry::LogResolvedPaths() const
    {
        ME_CORE_INFO("PathRegistry: EngineRoot='{}'", m_EngineRoot.string());
        ME_CORE_INFO(
            "PathRegistry: EngineConfig='{}'",
            m_EngineConfigFilePath.string());
        ME_CORE_INFO(
            "PathRegistry: EngineDefaultAssetsRoot='{}'",
            m_EngineDefaultAssetsRoot.string());
    }

}
