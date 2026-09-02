#include "Services/ToolbarModule.h"

#include "PlayMode/IPlayModeService.h"
#include "Shell/EditorInputHub.h"
#include "Shell/IEditorContext.h"

#include "imgui.h"

namespace minEngine
{
    void ToolbarModule::Register(IEditorContext& context)
    {
        EditorCommandBinding playCommand;
        playCommand.Name = "EnterPlay";
        playCommand.Chord = { ImGuiKey_F5, false, false, false };
        playCommand.CanExecute = [&context]() { return !context.GetPlayModeService().IsPlaying(); };
        playCommand.Execute = [&context]() { context.GetPlayModeService().EnterPlay(); };
        context.GetInputHub().RegisterGlobalCommand(std::move(playCommand));

        EditorCommandBinding stopCommand;
        stopCommand.Name = "StopPlay";
        stopCommand.Chord = { ImGuiKey_F5, false, true, false };
        stopCommand.CanExecute = [&context]() { return context.GetPlayModeService().IsPlaying(); };
        stopCommand.Execute = [&context]() { context.GetPlayModeService().Stop(); };
        context.GetInputHub().RegisterGlobalCommand(std::move(stopCommand));
    }

    void ToolbarModule::Shutdown()
    {
    }
}
