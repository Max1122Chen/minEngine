#pragma once

#include "Core.h"

#include <filesystem>
#include <optional>
#include <string>

namespace minEngine
{
    enum class ApplicationMode
    {
        Editor,
        Test,
    };

    enum class TestRunKind
    {
        Smoke,
        Full,
        SingleSuite,
    };

    struct CommandLineResult
    {
        ApplicationMode Mode = ApplicationMode::Editor;

        std::optional<std::filesystem::path> EngineConfigPath;
        std::optional<std::filesystem::path> EngineRootOverride;

        std::optional<std::filesystem::path> ProjectDescriptorPath;

        TestRunKind TestKind = TestRunKind::Smoke;
        std::string SuiteId;
        std::optional<std::string> ReflectionSuiteFilter;

        bool RequestedHelp = false;
        bool RequestedVersion = false;
        bool UsedLegacyMaterialIRFlag = false;
    };
}
