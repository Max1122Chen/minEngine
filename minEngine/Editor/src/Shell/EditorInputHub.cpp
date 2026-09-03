#include "Shell/EditorInputHub.h"

#include "Shell/EditorUndoRedoActions.h"
#include "Shell/EditorSubModule.h"
#include "Shell/IEditorContext.h"
#include "Viewport/EditorViewportClient.h"

namespace minEngine
{
    void EditorInputHub::Initialize(IEditorContext& context)
    {
        m_Context = &context;
        ClearGlobalCommands();
        ClearActiveSubModuleCommands();

        {
            EditorCommandBinding exitCommand;
            exitCommand.Name = "Exit";
            exitCommand.Chord = { ImGuiKey_F4, false, false, true };
            exitCommand.CanExecute = []() { return true; };
            exitCommand.Execute = [&context]() { context.RequestExit(); };
            RegisterGlobalCommand(std::move(exitCommand));
        }

        // Undo/Redo use ImGui::Shortcut(RouteGlobal) in ProcessGlobalUndoRedoShortcuts.
    }

    void EditorInputHub::Shutdown()
    {
        m_FocusedViewportClient = nullptr;
        m_Context = nullptr;
        ClearGlobalCommands();
        ClearActiveSubModuleCommands();
    }

    void EditorInputHub::RegisterGlobalCommand(EditorCommandBinding binding)
    {
        m_GlobalCommands.push_back(std::move(binding));
    }

    void EditorInputHub::ClearGlobalCommands()
    {
        m_GlobalCommands.clear();
    }

    void EditorInputHub::RegisterActiveSubModuleCommand(EditorCommandBinding binding)
    {
        m_ActiveSubModuleCommands.push_back(std::move(binding));
    }

    void EditorInputHub::ClearActiveSubModuleCommands()
    {
        m_ActiveSubModuleCommands.clear();
    }

    void EditorInputHub::SetFocusedViewportClient(EditorViewportClient* client)
    {
        m_FocusedViewportClient = client;
    }

    bool EditorInputHub::IsChordPressed(const EditorKeyChord& chord) const
    {
        if (chord.Key == ImGuiKey_None)
        {
            return false;
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (chord.Ctrl != io.KeyCtrl || chord.Shift != io.KeyShift || chord.Alt != io.KeyAlt)
        {
            return false;
        }

        return ImGui::IsKeyPressed(chord.Key, false);
    }

    bool EditorInputHub::ShouldProcessGlobalShortcuts(IEditorContext& context) const
    {
        (void)context;
        return !ImGui::GetIO().WantTextInput;
    }

    void EditorInputHub::ProcessCommandList(
        IEditorContext& context,
        const std::vector<EditorCommandBinding>& commands)
    {
        if (!ShouldProcessGlobalShortcuts(context))
        {
            return;
        }

        for (const EditorCommandBinding& binding : commands)
        {
            if (!binding.Execute)
            {
                continue;
            }

            if (binding.CanExecute && !binding.CanExecute())
            {
                continue;
            }

            if (IsChordPressed(binding.Chord))
            {
                binding.Execute();
                break;
            }
        }
    }

    void EditorInputHub::ProcessActiveSubModuleCommands(IEditorContext& context)
    {
        ProcessCommandList(context, m_ActiveSubModuleCommands);
    }

    void EditorInputHub::ProcessGlobalCommands(IEditorContext& context)
    {
        ProcessCommandList(context, m_GlobalCommands);
    }

    void EditorInputHub::ProcessGlobalUndoRedoShortcuts(IEditorContext& context)
    {
        if (!ShouldProcessGlobalShortcuts(context))
        {
            return;
        }

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
        {
            TryUndo(context);
        }

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal))
        {
            TryRedo(context);
        }
    }

    void EditorInputHub::ProcessViewportRouting(IEditorContext& context)
    {
        if (!m_FocusedViewportClient || !m_FocusedViewportClient->IsFocused())
        {
            return;
        }

        EditorSubModule* active = context.GetActiveSubModule();
        if (!active)
        {
            return;
        }

        active->RouteViewportInput(*m_FocusedViewportClient);
    }

    void EditorInputHub::ProcessInput(IEditorContext& context)
    {
        // During PIE: keep Play/Stop (and other global chords); suppress editor edit shortcuts
        // and undo/redo so they do not compete with gameplay InputSystem consumers.
        if (context.IsPlaying())
        {
            ProcessGlobalCommands(context);
            return;
        }

        ProcessViewportRouting(context);
        ProcessActiveSubModuleCommands(context);
        ProcessGlobalCommands(context);
        ProcessGlobalUndoRedoShortcuts(context);
    }
}
