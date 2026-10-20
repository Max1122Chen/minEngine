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
#include "EditorHostGameModule.h"
extern EDITOR_API minEngine::Application* minEngine::CreateApplication();

// BUILD-F04 S05: the game module identity of THIS editor host. libMaximumEditord.dll is ONE
// library shared by every editor host (MaximumEditor.exe, SampleEditor.exe, the external project
// editors), so "which game module am I" cannot be compiled into the library - it is compiled into
// the host by minengine_add_editor_executable() (GAME_MODULE -> MINENGINE_EDITOR_GAME_MODULE,
// no GAME_MODULE -> MINENGINE_EDITOR_ASSET_ONLY) and handed over below. This header is the single
// injection point every editor host already shares, which is what keeps the external repos' own
// EditorMain.cpp untouched (design section 9 hard acceptance item).
#if defined(MINENGINE_EDITOR_GAME_MODULE)
#define MINENGINE_EDITOR_HOST_MODULE_ID MINENGINE_EDITOR_GAME_MODULE
#else
#define MINENGINE_EDITOR_HOST_MODULE_ID minEngine::kEditorNoGameModule
#endif
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

#if defined(MINENGINE_EDITOR_HOST_LINKS_EDITOR_DLL)
    minEngine::SetHostEditorGameModule(MINENGINE_EDITOR_HOST_MODULE_ID);
#endif

    minEngine::Application* app = minEngine::CreateApplication();
    app->Initialize(argc, argv, *commandLine);
    app->Run();
    app->Shutdown();
    // BUILD-F04 S05: a refused start has to reach the shell (design O8), so the exit code comes
    // from the application instead of being hard-coded. Every pre-S05 application still returns 0.
    const int exitCode = app->GetExitCode();
    delete app;

    return exitCode;
}
