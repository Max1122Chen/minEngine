#pragma once

#include "Core.h"
#include "EditorWindow.h"

#include "imgui_node_editor.h"

namespace minEngine
{
    class Material;
    class MaterialEdGraph;

    class MaterialGraphWindow final : public EditorWindow
    {
    public:
        explicit MaterialGraphWindow(Editor& editor);
        ~MaterialGraphWindow() override;

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        EditorWindowSuite GetWindowSuite() const override { return EditorWindowSuite::MaterialEditing; }

        void OnDraw() override;

    private:
        void ShutdownNodeEditor();
        void EnsureNodeEditor();
        void DrawToolbar();
        void DrawCanvas();
        void DrawNodeEditor(Material& material, MaterialEdGraph& graph);
        void LayoutNodesIfNeeded(MaterialEdGraph& graph);
        void PushStoredPositionsToEditor(MaterialEdGraph& graph);
        void DrawNodes(MaterialEdGraph& graph);
        void DrawLinks(MaterialEdGraph& graph);
        void HandleCreateLink(MaterialEdGraph& graph);
        void HandleDeleteLink(MaterialEdGraph& graph);
        void SyncNodePositions(MaterialEdGraph& graph);
        bool TryConnectPins(ax::NodeEditor::PinId startPinId, ax::NodeEditor::PinId endPinId, MaterialEdGraph& graph);

        const std::string m_Id = "material_graph";
        const std::string m_Title = "Material Graph";

        ax::NodeEditor::EditorContext* m_NodeEditorContext = nullptr;
        MaterialEdGraph* m_BoundGraph = nullptr;
        bool m_NavigateToContentPending = false;
        bool m_PushStoredPositionsToEditor = false;
    };
}
