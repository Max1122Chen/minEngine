#pragma once

#include "Core.h"
#include "EditorWindow.h"
#include "Runtime/Core/GUID/GUID.h"

#include "imgui_node_editor.h"

#include <utility>
#include <vector>

namespace minEngine
{
    class Material;
    class MaterialEdGraph;
    class MaterialEdGraphNode;
    class MaterialGraphNodeDef;

    class MaterialGraphWindow final : public EditorWindow
    {
    public:
        explicit MaterialGraphWindow(IEditorContext& context);
        ~MaterialGraphWindow() override;

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override;

        void OnDraw() override;

    private:
        void ShutdownNodeEditor();
        void EnsureNodeEditor();
        void DrawToolbar();
        void DrawCanvas();
        void DrawNodeEditor(Material& material, MaterialEdGraph& graph);
        void LayoutNodesIfNeeded(MaterialEdGraph& graph);
        void PushStoredPositionsToEditor(MaterialEdGraph& graph);
        void DrawPinIcon(const ImColor& color);
        void DrawNodeBody(Material& material, MaterialEdGraphNode& node, MaterialGraphNodeDef* nodeDef);
        void DrawNodes(Material& material, MaterialEdGraph& graph);
        void SyncSelectionFromEditor();
        void DrawLinks(MaterialEdGraph& graph);
        void HandleCreateLink(Material& material, MaterialEdGraph& graph);
        void HandleDeleteLink(MaterialEdGraph& graph);
        void SyncNodePositions(MaterialEdGraph& graph);
        bool TryConnectPins(
            ax::NodeEditor::PinId startPinId,
            ax::NodeEditor::PinId endPinId,
            Material& material,
            MaterialEdGraph& graph);
        bool ResolveConnectPins(
            ax::NodeEditor::PinId startPinId,
            ax::NodeEditor::PinId endPinId,
            Material& material,
            MaterialEdGraph& graph,
            MaterialEdGraphNode*& outFromNode,
            int32_t& outFromOutputIndex,
            MaterialEdGraphNode*& outToNode,
            int32_t& outToInputIndex);
        void CommitNodePositionDragIfNeeded(MaterialEdGraph& graph);

        const std::string m_Id = "material_graph";
        const std::string m_Title = "Material Graph";

        ax::NodeEditor::EditorContext* m_NodeEditorContext = nullptr;
        MaterialEdGraph* m_BoundGraph = nullptr;
        bool m_PushStoredPositionsToEditor = false;
        bool m_PositionDragCaptured = false;
        std::vector<std::pair<GUID, ImVec2>> m_PositionDragBefore;
    };
}
