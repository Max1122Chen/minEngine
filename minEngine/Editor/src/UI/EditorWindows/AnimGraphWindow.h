#pragma once

#include "Core.h"
#include "EditorWindow.h"

#include "imgui_node_editor.h"

#include "Runtime/Function/Animation/AnimationGraph.h"

namespace minEngine
{
    class AnimGraphWindow final : public EditorWindow
    {
    public:
        explicit AnimGraphWindow(IEditorContext& context);
        ~AnimGraphWindow() override;

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override;

        void OnDraw() override;

    private:
        void ShutdownNodeEditor();
        void EnsureNodeEditor();
        void DrawToolbar();
        void DrawCanvas();
        void DrawNodeEditor(AnimationGraph& graph);
        void LayoutNodesIfNeeded(AnimStateMachine& stateMachine);
        void PushStoredPositionsToEditor(AnimStateMachine& stateMachine);
        void DrawNodes(AnimStateMachine& stateMachine);
        void DrawLinks(AnimStateMachine& stateMachine);
        void SyncSelectionFromEditor(AnimStateMachine& stateMachine);
        void HandleCreateLink(AnimationGraph& graph);
        void HandleDelete(AnimationGraph& graph);
        void SyncNodePositions(AnimStateMachine& stateMachine);
        void DrawBackgroundContextMenu(AnimationGraph& graph);

        const std::string m_Id = "anim_graph";
        const std::string m_Title = "Anim Graph";

        ax::NodeEditor::EditorContext* m_NodeEditorContext = nullptr;
        AnimationGraph* m_BoundGraph = nullptr;
        bool m_PushStoredPositionsToEditor = false;
    };
}