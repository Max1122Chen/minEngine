#include "Core.h"
#include "Application.h"
#include "Runtime/Core/CLI/ApplicationCommandLine.h"
#include "Runtime/Core/CLI/CommandLineExitCode.h"
#include "Runtime/Test/TestExecutableForward.h"

// BUILD-F04 S02 / Q3: an editor host links the editor DLL instead of compiling the editor in.
// minengine_add_editor_executable() defines MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL on the host, so
// the entry symbol resolves through the editor DLL import library (dllimport) while every other
// host keeps the plain extern. The host main source stays untouched (external repos too).
#if defined(MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL)
#include "EditorAPI.h"
extern EDITOR_API minEngine::Application* minEngine::CreateApplication();
#else
extern minEngine::Application* minEngine::CreateApplication();
#endif

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
