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
