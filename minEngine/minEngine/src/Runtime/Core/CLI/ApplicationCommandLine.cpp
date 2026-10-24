#include "ApplicationCommandLine.h"

#include "CLI11.hpp"
#include "Runtime/Core/EngineVersion.h"
#include "Runtime/Core/ProductBranding.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/SceneRendererKind.h"

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

        std::string ToLowerAscii(std::string value)
        {
            for (char& ch : value)
            {
                if (ch >= 'A' && ch <= 'Z')
                {
                    ch = static_cast<char>(ch - 'A' + 'a');
                }
            }
            return value;
        }

        std::optional<RHIBackendType> ParseRHIBackend(const std::string& value)
        {
            const std::string lowered = ToLowerAscii(value);
            if (lowered == "opengl" || lowered == "gl")
            {
                return RHIBackendType::OpenGL;
            }
            if (lowered == "vulkan" || lowered == "vk")
            {
                return RHIBackendType::Vulkan;
            }
            return std::nullopt;
        }

        std::optional<SceneRendererKind> ParseSceneRenderer(const std::string& value)
        {
            const std::string lowered = ToLowerAscii(value);
            if (lowered == "forward" || lowered == "rdg")
            {
                return SceneRendererKind::Forward;
            }
            if (lowered == "manual" || lowered == "handpass")
            {
                return SceneRendererKind::Manual;
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

        CLI::App app(FormatEditorWindowTitleBase());
        app.positionals_at_end(true);
        app.set_version_flag(
            "--version",
            FormatProductVersionLine(GetEngineVersion().ToString()));

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

        std::string rhiBackend;
        app.add_option("--rhi", rhiBackend, "Graphics backend: opengl|vulkan (aliases gl|vk). Default: opengl")
            ->option_text("<opengl|vulkan>");

        std::string sceneRenderer;
        app.add_option(
            "--renderer",
            sceneRenderer,
            "Scene renderer: forward|manual (aliases rdg|handpass). Default: forward")
            ->option_text("<forward|manual>");

        std::string frameThreads;
        app.add_option(
            "--frame-threads",
            frameThreads,
            "Frame thread mode: single|multi (env MINENGINE_FRAME_THREADS). Default: single")
            ->option_text("<single|multi>");

        std::string automationPipe;
        app.add_option(
            "--automation-pipe",
            automationPipe,
            "Enable Editor Automation Host on this Named Pipe (include pid/random; do not use a fixed global name)")
            ->option_text("<name>");
        std::string automationReadyFile;
        app.add_option(
            "--automation-ready-file",
            automationReadyFile,
            "Path written after Listen (+ optional --open-scene); primary Spawn ready channel")
            ->option_text("<path>");
        std::string openSceneAssetPath;
        app.add_option(
            "--open-scene",
            openSceneAssetPath,
            "Open this Scene after project load (project-relative asset path, e.g. Scenes/test.mescene)")
            ->option_text("<asset-path>");

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

        if (!rhiBackend.empty())
        {
            const std::optional<RHIBackendType> parsedBackend = ParseRHIBackend(rhiBackend);
            if (!parsedBackend.has_value())
            {
                std::fprintf(
                    stderr,
                    "Invalid --rhi '%s'. Expected opengl|vulkan (or gl|vk).\n",
                    rhiBackend.c_str());
                s_LastExitCode = CommandLineExitCode::UsageError;
                return std::nullopt;
            }
            result.RHIBackend = *parsedBackend;
        }

        if (!sceneRenderer.empty())
        {
            const std::optional<SceneRendererKind> parsedRenderer = ParseSceneRenderer(sceneRenderer);
            if (!parsedRenderer.has_value())
            {
                std::fprintf(
                    stderr,
                    "Invalid --renderer '%s'. Expected forward|manual (aliases rdg|handpass).\n",
                    sceneRenderer.c_str());
                s_LastExitCode = CommandLineExitCode::UsageError;
                return std::nullopt;
            }
            result.SceneRenderer = *parsedRenderer;
        }

        if (!frameThreads.empty())
        {
            const std::optional<FrameThreadMode> parsedMode = FrameThreading::TryParseMode(frameThreads);
            if (!parsedMode.has_value())
            {
                std::fprintf(
                    stderr,
                    "Invalid --frame-threads '%s'. Expected single|multi.\n",
                    frameThreads.c_str());
                s_LastExitCode = CommandLineExitCode::UsageError;
                return std::nullopt;
            }
            result.FrameThreads = parsedMode;
        }

        if (app.got_subcommand("test"))
        {
            if (!automationPipe.empty() || !automationReadyFile.empty() || !openSceneAssetPath.empty())
            {
                std::fprintf(
                    stderr,
                    "Automation flags (--automation-pipe / --automation-ready-file / --open-scene) "
                    "are only valid in Editor mode, not with 'test'.\n");
                s_LastExitCode = CommandLineExitCode::UsageError;
                return std::nullopt;
            }

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

        if (!automationReadyFile.empty() && automationPipe.empty())
        {
            std::fprintf(
                stderr,
                "--automation-ready-file requires --automation-pipe.\n");
            s_LastExitCode = CommandLineExitCode::UsageError;
            return std::nullopt;
        }

        if (!automationPipe.empty())
        {
            result.AutomationPipeName = automationPipe;
        }
        if (!automationReadyFile.empty())
        {
            result.AutomationReadyFilePath = std::filesystem::path(automationReadyFile);
        }
        if (!openSceneAssetPath.empty())
        {
            result.OpenSceneAssetPath = openSceneAssetPath;
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
