#include "Core.h"
#include "Application.h"
#include "Runtime/Core/CLI/ApplicationCommandLine.h"
#include "Runtime/Core/CLI/CommandLineExitCode.h"
#include "Runtime/Core/CLI/CommandLineResult.h"
#include "Runtime/Test/TestRunner.h"

extern minEngine::Application* minEngine::CreateApplication();

namespace minEngine
{
    static int ExitCodeFrom(CommandLineExitCode exitCode)
    {
        return static_cast<int>(exitCode);
    }
}

int main(int argc, char** argv)
{
    if (minEngine::TestRunner::ContainsLegacyTestFlag(argc, argv))
    {
        minEngine::CommandLineResult legacyPlaceholder;
        return minEngine::ExitCodeFrom(
            minEngine::TestRunner::Run(legacyPlaceholder, argc, argv));
    }

    const std::optional<minEngine::CommandLineResult> commandLine =
        minEngine::ApplicationCommandLine::TryParse(argc, argv);
    if (!commandLine.has_value())
    {
        return minEngine::ExitCodeFrom(minEngine::ApplicationCommandLine::GetLastExitCode());
    }

    if (commandLine->RequestedHelp)
    {
        return 0;
    }

    if (commandLine->RequestedVersion)
    {
        return 0;
    }

    if (commandLine->Mode == minEngine::ApplicationMode::Test
        || minEngine::TestRunner::ContainsLegacyTestFlag(argc, argv))
    {
        return minEngine::ExitCodeFrom(minEngine::TestRunner::Run(*commandLine, argc, argv));
    }

    minEngine::Application* app = minEngine::CreateApplication();
    app->Initialize(argc, argv, *commandLine);
    app->Run();
    app->Shutdown();
    delete app;

    return 0;
}
