#include "ApplicationCommandLine.h"

#include "CLI11.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace minEngine
{
    CommandLineExitCode ApplicationCommandLine::s_LastExitCode = CommandLineExitCode::Success;

    namespace
    {
        constexpr const char* kLegacyMaterialIRTestFlag = "--material-ir-test";

        bool ArgEquals(char* arg, const char* literal)
        {
            return arg != nullptr && std::string_view(arg) == literal;
        }

        bool ContainsLegacyMaterialIRFlag(int argc, char** argv)
        {
            for (int argIndex = 1; argIndex < argc; ++argIndex)
            {
                if (ArgEquals(argv[argIndex], kLegacyMaterialIRTestFlag))
                {
                    return true;
                }
            }
            return false;
        }

        CommandLineResult BuildLegacyMaterialIRResult(int argc, char** argv)
        {
            std::fprintf(
                stderr,
                "Warning: '%s' is deprecated; use 'test material-ir' instead.\n",
                kLegacyMaterialIRTestFlag);

            CommandLineResult result;
            result.Mode = ApplicationMode::Test;
            result.TestKind = TestRunKind::SingleSuite;
            result.SuiteId = "material-ir";
            result.UsedLegacyMaterialIRFlag = true;

            for (int argIndex = 1; argIndex < argc; ++argIndex)
            {
                if (argv[argIndex] == nullptr)
                {
                    continue;
                }

                const std::string_view arg(argv[argIndex]);
                constexpr std::string_view kEngineConfigPrefix = "--engine-config=";
                if (arg.size() > kEngineConfigPrefix.size() &&
                    arg.substr(0, kEngineConfigPrefix.size()) == kEngineConfigPrefix)
                {
                    result.EngineConfigPath = std::filesystem::path(
                        std::string(arg.substr(kEngineConfigPrefix.size())));
                }

                constexpr std::string_view kEngineRootPrefix = "--engine-root=";
                if (arg.size() > kEngineRootPrefix.size() &&
                    arg.substr(0, kEngineRootPrefix.size()) == kEngineRootPrefix)
                {
                    result.EngineRootOverride =
                        std::filesystem::path(std::string(arg.substr(kEngineRootPrefix.size())));
                }
            }

            return result;
        }

        std::optional<TestRunKind> ParseTestTargetKind(const std::string& target)
        {
            if (target == "smoke")
            {
                return TestRunKind::Smoke;
            }
            if (target == "full")
            {
                return TestRunKind::Full;
            }
            return std::nullopt;
        }
    }

    CommandLineExitCode ApplicationCommandLine::GetLastExitCode()
    {
        return s_LastExitCode;
    }

    std::optional<CommandLineResult> ApplicationCommandLine::TryParse(int argc, char** argv)
    {
        s_LastExitCode = CommandLineExitCode::Success;

        if (ContainsLegacyMaterialIRFlag(argc, argv))
        {
            return BuildLegacyMaterialIRResult(argc, argv);
        }

        CLI::App app("minEngine Editor");
        app.set_version_flag("--version", "minEngine (CLI-F01)");

        std::string engineConfigPath;
        std::string engineRootOverride;
        app.add_option("--engine-config", engineConfigPath, "Path to EngineConfig.meconfig")
            ->option_text("<path>");
        app.add_option("--engine-root", engineRootOverride, "Override engine root after config load")
            ->option_text("<path>");

        std::string projectPath;
        app.add_option("--project,-p", projectPath, "Project descriptor (.meproject)")
            ->option_text("<path>");
        std::string projectPositional;
        app.add_option("meproject", projectPositional, "Project descriptor (.meproject)")
            ->option_text("<path>");

        CLI::App& testCommand = *app.add_subcommand("test", "Headless test mode (no editor window)");
        std::string testTarget;
        testCommand.add_option("target", testTarget, "smoke | full | <suite-id>")
            ->required()
            ->option_text("<target>");
        std::string reflectionSuiteFilter;
        testCommand.add_option("--suite", reflectionSuiteFilter, "Reflection-function suite filter")
            ->option_text("<name>");

        try
        {
            app.parse(argc, argv);
        }
        catch (const CLI::ParseError& parseError)
        {
            if (dynamic_cast<const CLI::CallForHelp*>(&parseError) != nullptr)
            {
                std::fprintf(stdout, "%s", app.help().c_str());
                CommandLineResult helpResult;
                helpResult.RequestedHelp = true;
                return helpResult;
            }

            if (dynamic_cast<const CLI::CallForVersion*>(&parseError) != nullptr)
            {
                std::fprintf(stdout, "%s\n", app.version().c_str());
                CommandLineResult versionResult;
                versionResult.RequestedVersion = true;
                return versionResult;
            }

            app.exit(parseError);
            s_LastExitCode = CommandLineExitCode::UsageError;
            return std::nullopt;
        }

        CommandLineResult result;

        if (!engineConfigPath.empty())
        {
            result.EngineConfigPath = std::filesystem::path(engineConfigPath);
        }
        if (!engineRootOverride.empty())
        {
            result.EngineRootOverride = std::filesystem::path(engineRootOverride);
        }

        if (app.got_subcommand("test"))
        {
            result.Mode = ApplicationMode::Test;

            if (const std::optional<TestRunKind> kind = ParseTestTargetKind(testTarget))
            {
                result.TestKind = *kind;
            }
            else
            {
                result.TestKind = TestRunKind::SingleSuite;
                result.SuiteId = testTarget;
            }

            if (!reflectionSuiteFilter.empty())
            {
                result.ReflectionSuiteFilter = reflectionSuiteFilter;
            }

            return result;
        }

        if (!projectPath.empty())
        {
            result.ProjectDescriptorPath = std::filesystem::path(projectPath);
        }
        else if (!projectPositional.empty())
        {
            result.ProjectDescriptorPath = std::filesystem::path(projectPositional);
        }

        return result;
    }
}
