#include "Shell/EditorInputHub.h"

#include "Shell/EditorCommandHistory.h"
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

        {
            EditorCommandBinding undoCommand;
            undoCommand.Name = "Undo";
            undoCommand.Chord = { ImGuiKey_Z, true, false, false };
            undoCommand.CanExecute = [&context]() { return context.GetCommandHistory().CanUndo(); };
            undoCommand.Execute = [&context]() { context.GetCommandHistory().Undo(); };
            RegisterGlobalCommand(std::move(undoCommand));
        }

        {
            EditorCommandBinding redoCommand;
            redoCommand.Name = "Redo";
            redoCommand.Chord = { ImGuiKey_Y, true, false, false };
            redoCommand.CanExecute = [&context]() { return context.GetCommandHistory().CanRedo(); };
            redoCommand.Execute = [&context]() { context.GetCommandHistory().Redo(); };
            RegisterGlobalCommand(std::move(redoCommand));
        }
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
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureKeyboard)
        {
            return true;
        }

        if (m_FocusedViewportClient && m_FocusedViewportClient->IsFocused())
        {
            return false;
        }

        return false;
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
        ProcessViewportRouting(context);
        ProcessActiveSubModuleCommands(context);
        ProcessGlobalCommands(context);
    }
}
