#include "Core.h"
#include "Application.h"
#include "Runtime/Core/CLI/ApplicationCommandLine.h"
#include "Runtime/Core/CLI/CommandLineExitCode.h"
#include "Runtime/Test/TestExecutableForward.h"

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

    if (commandLine->Mode == minEngine::ApplicationMode::Test)
    {
        return minEngine::ForwardToMinEngineTestsExecutable(argc, argv);
    }

    minEngine::Application* app = minEngine::CreateApplication();
    app->Initialize(argc, argv, *commandLine);
    app->Run();
    app->Shutdown();
    delete app;

    return 0;
}
