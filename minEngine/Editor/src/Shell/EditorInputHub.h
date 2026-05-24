#pragma once

#include "Core.h"

#include "imgui.h"

#include <functional>
#include <string>
#include <vector>

namespace minEngine
{
    class EditorViewportClient;
    class IEditorContext;

    struct EditorKeyChord
    {
        ImGuiKey Key = ImGuiKey_None;
        bool Ctrl = false;
        bool Shift = false;
        bool Alt = false;
    };

    struct EditorCommandBinding
    {
        std::string Name;
        EditorKeyChord Chord;
        std::function<bool()> CanExecute;
        std::function<void()> Execute;
    };

    /** Global / viewport input routing (UE FUICommandList + FEditorModeTools style stub). */
    class EditorInputHub
    {
    public:
        void Initialize(IEditorContext& context);
        void Shutdown();

        void RegisterGlobalCommand(EditorCommandBinding binding);
        void ClearGlobalCommands();

        void RegisterActiveSubModuleCommand(EditorCommandBinding binding);
        void ClearActiveSubModuleCommands();

        void SetFocusedViewportClient(EditorViewportClient* client);
        EditorViewportClient* GetFocusedViewportClient() const { return m_FocusedViewportClient; }

        /** Call once per frame after ImGui UI has been built for the frame. */
        void ProcessInput(IEditorContext& context);

    private:
        bool IsChordPressed(const EditorKeyChord& chord) const;
        bool ShouldProcessGlobalShortcuts(IEditorContext& context) const;
        void ProcessActiveSubModuleCommands(IEditorContext& context);
        void ProcessGlobalCommands(IEditorContext& context);
        void ProcessGlobalUndoRedoShortcuts(IEditorContext& context);
        void ProcessViewportRouting(IEditorContext& context);
        void ProcessCommandList(IEditorContext& context, const std::vector<EditorCommandBinding>& commands);

        IEditorContext* m_Context = nullptr;
        EditorViewportClient* m_FocusedViewportClient = nullptr;
        std::vector<EditorCommandBinding> m_GlobalCommands;
        std::vector<EditorCommandBinding> m_ActiveSubModuleCommands;
    };
}
