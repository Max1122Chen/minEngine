#include "MaterialGraphWindow.h"

#include "Shell/EditorContextHelpers.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemeScope.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTheme.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include "Material/MaterialEditor.h"
#include "Material/MaterialEditorSession.h"
#include "Material/MaterialGraphIds.h"
#include "Material/MaterialCompileDiagnosticsDrawer.h"
#include "Material/MaterialGraphNodeRegistry.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialEdGraphNode.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <vector>

namespace minEngine
{
    namespace Ed = ax::NodeEditor;

    namespace
    {
        constexpr float kNodeWidth = MaterialGraphNodeRegistry::kNodeContentWidth;
        constexpr float kHeaderHeight = 24.0f;
        constexpr float kPinRadius = 4.5f;
        constexpr float kPinTextGap = 6.0f;
        constexpr float kRowHeight = 18.0f;

        ImU32 PinColor(bool connected)
        {
            return connected ? IM_COL32(120, 200, 255, 255) : IM_COL32(150, 150, 150, 255);
        }
    }

    void MaterialGraphWindow::DrawNodeBody(Material& material, MaterialEdGraphNode& node, MaterialGraphNodeDef* nodeDef)
    {
        if (!nodeDef)
        {
            return;
        }

        const bool isMaterialOutput = nodeDef->IsMaterialOutputNode();
        const MaterialShadingModel shadingModel = material.m_ShadingModel;
        const MaterialBlendMode blendMode = material.m_BlendMode;

        int inputCount = 0;
        while (nodeDef->GetInput(inputCount))
        {
            ++inputCount;
        }

        int outputCount = 0;
        while (nodeDef->GetOutput(outputCount))
        {
            ++outputCount;
        }

        std::vector<int> visibleInputIndices;
        visibleInputIndices.reserve(static_cast<size_t>(inputCount));
        for (int inputIndex = 0; inputIndex < inputCount; ++inputIndex)
        {
            const MaterialGraphNodeDef::Input* input = nodeDef->GetInput(inputIndex);
            if (input == nullptr)
            {
                continue;
            }

            if (isMaterialOutput)
            {
                const MaterialPropertyPinVisibility visibility =
                    MaterialCapabilityUtil::GetMaterialOutputInputVisibility(
                        input->Name.c_str(),
                        shadingModel,
                        blendMode);

                if (visibility == MaterialPropertyPinVisibility::Hidden)
                {
                    continue;
                }
            }

            visibleInputIndices.push_back(inputIndex);
        }

        const int visibleInputCount = static_cast<int>(visibleInputIndices.size());
        const int maxPinCount = std::max(visibleInputCount, outputCount);
        const float halfWidth = kNodeWidth * 0.5f;
        constexpr float kPinIconSize = 10.0f;

        for (int row = 0; row < maxPinCount; ++row)
        {
            ImGui::PushID(row);
            const float rowStartX = ImGui::GetCursorPosX();

            if (row < visibleInputCount)
            {
                const int inputIndex = visibleInputIndices[static_cast<size_t>(row)];
                MaterialGraphNodeDef::Input* input = nodeDef->GetInput(inputIndex);
                MaterialPropertyPinVisibility visibility = MaterialPropertyPinVisibility::Active;
                if (isMaterialOutput)
                {
                    visibility = MaterialCapabilityUtil::GetMaterialOutputInputVisibility(
                        input->Name.c_str(),
                        shadingModel,
                        blendMode);
                }

                const Ed::PinId pinId = MaterialGraphIds::ToPinId(&node, Ed::PinKind::Input, inputIndex);
                const bool pinDisabled = visibility == MaterialPropertyPinVisibility::Disabled;
                const ImColor inputPinColor =
                    pinDisabled ? ImColor(110, 110, 110) : ImColor(150, 220, 150);

                if (pinDisabled)
                {
                    ImGui::BeginDisabled();
                }

                Ed::BeginPin(pinId, Ed::PinKind::Input);
                DrawPinIcon(inputPinColor);
                ImGui::SameLine(0.0f, kPinTextGap);
                ImGui::TextUnformatted(input->Name.c_str());
                Ed::EndPin();

                if (pinDisabled)
                {
                    ImGui::EndDisabled();
                }
            }
            else
            {
                ImGui::Dummy(ImVec2(halfWidth, kRowHeight));
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(rowStartX + halfWidth);

            if (row < outputCount)
            {
                MaterialGraphNodeDef::Output* output = nodeDef->GetOutput(row);
                const Ed::PinId pinId = MaterialGraphIds::ToPinId(&node, Ed::PinKind::Output, row);
                Ed::BeginPin(pinId, Ed::PinKind::Output);

                const float labelWidth = ImGui::CalcTextSize(output->Name.c_str()).x;
                const float contentWidth = labelWidth + kPinTextGap + kPinIconSize;
                const float offset = halfWidth - contentWidth;
                if (offset > 0.0f)
                {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
                }

                ImGui::TextUnformatted(output->Name.c_str());
                ImGui::SameLine(0.0f, kPinTextGap);
                DrawPinIcon(ImColor(220, 180, 80));
                Ed::EndPin();
            }
            else
            {
                ImGui::Dummy(ImVec2(halfWidth, kRowHeight));
            }

            ImGui::PopID();
        }
    }

    MaterialGraphWindow::MaterialGraphWindow(IEditorContext& context)
        : EditorWindow(context)
    {
        SetOpen(false);
    }

    MaterialGraphWindow::~MaterialGraphWindow()
    {
        ShutdownNodeEditor();
    }

    void MaterialGraphWindow::ShutdownNodeEditor()
    {
        if (m_NodeEditorContext)
        {
            Ed::DestroyEditor(m_NodeEditorContext);
            m_NodeEditorContext = nullptr;
        }

        m_BoundGraph = nullptr;
        MaterialGraphIds::Reset();
    }

    void MaterialGraphWindow::EnsureNodeEditor()
    {
        if (m_NodeEditorContext)
        {
            return;
        }

        Ed::Config config;
        config.SettingsFile = nullptr;
        config.SaveSettings = nullptr;
        config.LoadSettings = nullptr;
        config.SaveNodeSettings = nullptr;
        config.LoadNodeSettings = nullptr;
        m_NodeEditorContext = Ed::CreateEditor(&config);
    }

    void MaterialGraphWindow::OnDraw()
    {
        if (!EditorWindowTypography::BeginPanel(
                m_Context,
                m_Title.c_str(),
                nullptr,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            return;
        }

        EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);

        MaterialEditor* materialEditor = GetMaterialEditor(&m_Context);
        if (!materialEditor)
        {
            ImGui::End();
            return;
        }
        bool rebindGraph = false;
        if (materialEditor->ConsumeGraphCanvasInvalidation(rebindGraph))
        {
            if (rebindGraph)
            {
                MaterialGraphIds::Reset();
                materialEditor->ClearSelectedEdNode();
                m_BoundGraph = nullptr;
            }

            m_PushStoredPositionsToEditor = true;
        }

        DrawToolbar();
        ImGui::Separator();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("MaterialGraphCanvas", avail, true, ImGuiWindowFlags_NoScrollbar);
        DrawCanvas();
        ImGui::EndChild();

        ImGui::End();
    }

    void MaterialGraphWindow::DrawToolbar()
    {
        MaterialEditor* materialEditor = GetMaterialEditor(&m_Context);
        if (!materialEditor)
        {
            return;
        }
        const MaterialEditorSession& session = materialEditor->GetSession();
        const auto& materialMetas = materialEditor->GetMaterialMetas();
        const int selectedIndex = materialEditor->GetSelectedMaterialIndex();

        const char* previewLabel = "No material";
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(materialMetas.size()))
        {
            previewLabel = materialMetas[static_cast<size_t>(selectedIndex)]->AssetName.c_str();
        }
        else if (session.HasOpenMaterial())
        {
            previewLabel = session.AssetPath.c_str();
        }

        ImGui::SetNextItemWidth(280.0f);
        if (materialMetas.empty())
        {
            ImGui::TextDisabled("No .memtl assets found (open a project with Assets scanned).");
        }
        else if (ImGui::BeginCombo("##MaterialAssetCombo", previewLabel))
        {
            for (int i = 0; i < static_cast<int>(materialMetas.size()); ++i)
            {
                const AssetMeta* meta = materialMetas[static_cast<size_t>(i)];
                ImGui::PushID(i);
                const bool selected = (i == selectedIndex);
                if (ImGui::Selectable(meta->AssetName.c_str(), selected))
                {
                    materialEditor->OpenSession(meta);
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Compile"))
        {
            materialEditor->CompileActiveMaterial();
        }

        if (session.HasOpenMaterial() && session.MaterialAsset)
        {
            const size_t diagnosticCount = session.MaterialAsset->m_LastCompileDiagnostics.size();
            if (diagnosticCount > 0)
            {
                ImGui::SameLine();
                const EditorAppearance& appearance = m_Context.GetEditorAppearance();
                const EditorSemanticColors& semanticColors = appearance.GetSemanticColors();
                ImGui::TextColored(appearance.GetDisplayColor(semanticColors.DiagnosticWarning),
                                   "(%zu)",
                                   diagnosticCount);
            }
        }

        ImGui::SameLine();
        if (!session.HasOpenMaterial())
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button(session.Dirty ? "Save *" : "Save"))
        {
            materialEditor->SaveActiveMaterial();
        }

        if (!session.HasOpenMaterial())
        {
            ImGui::EndDisabled();
        }

        if (session.HasOpenMaterial() && session.MaterialAsset)
        {
            MaterialCompileDiagnosticsDrawer::Draw(
                *session.MaterialAsset,
                m_Context.GetEditorAppearance(),
                false);
        }
    }

    void MaterialGraphWindow::DrawCanvas()
    {
        MaterialEditor* materialEditor = GetMaterialEditor(&m_Context);
        if (!materialEditor)
        {
            return;
        }
        const MaterialEditorSession& session = materialEditor->GetSession();
        if (!session.HasOpenMaterial() || !session.MaterialAsset || !session.MaterialAsset->m_Graph)
        {
            ImGui::TextUnformatted("Open a material asset to edit its graph.");
            return;
        }

        EnsureNodeEditor();
        DrawNodeEditor(*session.MaterialAsset, *session.MaterialAsset->m_Graph);
    }

    void MaterialGraphWindow::DrawNodeEditor(Material& material, MaterialEdGraph& graph)
    {
        Ed::SetCurrentEditor(m_NodeEditorContext);

        if (m_BoundGraph != &graph)
        {
            MaterialGraphIds::Reset();
            if (MaterialEditor* editor = GetMaterialEditor(&m_Context))
            {
                editor->ClearSelectedEdNode();
            }
            m_BoundGraph = &graph;
            m_PushStoredPositionsToEditor = true;
        }

        Ed::Begin("MaterialGraphEditor", ImVec2(0.0f, 0.0f));

        if (m_PushStoredPositionsToEditor)
        {
            PushStoredPositionsToEditor(graph);
            m_PushStoredPositionsToEditor = false;
        }

        DrawNodes(material, graph);
        DrawLinks(graph);
        HandleCreateLink(material, graph);
        HandleDeleteLink(graph);
        SyncSelectionFromEditor();
        SyncNodePositions(graph);

        Ed::End();
        Ed::SetCurrentEditor(nullptr);
    }

    void MaterialGraphWindow::LayoutNodesIfNeeded(MaterialEdGraph& graph)
    {
        bool needsLayout = true;
        for (const std::shared_ptr<MaterialEdGraphNode>& node : graph.m_Nodes)
        {
            if (!node)
            {
                continue;
            }

            if (node->m_EditorPosX != 0.0f || node->m_EditorPosY != 0.0f)
            {
                needsLayout = false;
                break;
            }
        }

        if (!needsLayout)
        {
            return;
        }

        constexpr float kSpacingX = 300.0f;
        constexpr float kSpacingY = 140.0f;
        constexpr int kColumns = 3;

        int layoutIndex = 0;
        for (const std::shared_ptr<MaterialEdGraphNode>& node : graph.m_Nodes)
        {
            if (!node)
            {
                continue;
            }

            const int column = layoutIndex % kColumns;
            const int row = layoutIndex / kColumns;
            node->m_EditorPosX = static_cast<float>(column) * kSpacingX;
            node->m_EditorPosY = static_cast<float>(row) * kSpacingY;
            ++layoutIndex;
        }

        if (layoutIndex > 0)
        {
            if (MaterialEditor* editor = GetMaterialEditor(&m_Context))
            {
                editor->GetSession().Dirty = true;
            }
        }
    }

    void MaterialGraphWindow::PushStoredPositionsToEditor(MaterialEdGraph& graph)
    {
        LayoutNodesIfNeeded(graph);

        for (const std::shared_ptr<MaterialEdGraphNode>& node : graph.m_Nodes)
        {
            if (!node)
            {
                continue;
            }

            const ImVec2 position(node->m_EditorPosX, node->m_EditorPosY);
            Ed::SetNodePosition(MaterialGraphIds::ToNodeId(node.get()), position);
        }
    }

    void MaterialGraphWindow::DrawNodes(Material& material, MaterialEdGraph& graph)
    {
        MaterialGraphNodeRegistry::EnsureRegistered();

        int nodeDrawIndex = 0;
        for (const std::shared_ptr<MaterialEdGraphNode>& nodePtr : graph.m_Nodes)
        {
            if (!nodePtr)
            {
                continue;
            }

            MaterialEdGraphNode& node = *nodePtr;
            MaterialGraphNodeDef* nodeDef = node.GetNodeDef();
            const MaterialGraphNodeStyle style = MaterialGraphNodeRegistry::GetStyle(nodeDef);

            const Ed::NodeId nodeId = MaterialGraphIds::ToNodeId(&node);
            ImGui::PushID(nodeDrawIndex++);
            Ed::BeginNode(nodeId);
            ImGui::PushItemWidth(kNodeWidth);

            const char* title = node.m_Title.empty() ? style.DisplayName : node.m_Title.c_str();
            {
                EditorThemeScope nodeTitleTheme = EditorWindowTheme::PrimaryText(m_Context.GetEditorAppearance());
                ImGui::TextUnformatted(title);
            }

            MaterialEditor* materialEditor = GetMaterialEditor(&m_Context);
            if (!materialEditor)
            {
                ImGui::PopItemWidth();
                Ed::EndNode();
                ImGui::PopID();
                continue;
            }

            if (MaterialGraphNodeRegistry::DrawNode(node))
            {
                materialEditor->NotifyGraphChanged();
            }

            ImGui::Spacing();
            DrawNodeBody(material, node, nodeDef);
            ImGui::PopItemWidth();
            Ed::EndNode();
            ImGui::PopID();
        }
    }

    void MaterialGraphWindow::DrawPinIcon(const ImColor& color)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImVec2 cursor = ImGui::GetCursorScreenPos();

        constexpr float radius = 5.0f;

        drawList->AddCircleFilled(
            ImVec2(cursor.x + radius, cursor.y + radius),
            radius,
            color);

        ImGui::Dummy(ImVec2(radius * 2, radius * 2));
    }

    void MaterialGraphWindow::SyncSelectionFromEditor()
    {
        MaterialEditor* materialEditor = GetMaterialEditor(&m_Context);
        if (!materialEditor)
        {
            return;
        }
        const int selectedCount = Ed::GetSelectedObjectCount();
        if (selectedCount != 1)
        {
            materialEditor->ClearSelectedEdNode();
            return;
        }

        Ed::NodeId selectedNodeId;
        const int nodeCount = Ed::GetSelectedNodes(&selectedNodeId, 1);
        if (nodeCount != 1)
        {
            materialEditor->ClearSelectedEdNode();
            return;
        }

        materialEditor->SetSelectedEdNode(MaterialGraphIds::FromNodeId(selectedNodeId));
    }

    void MaterialGraphWindow::DrawLinks(MaterialEdGraph& graph)
    {
        for (const std::shared_ptr<MaterialEdGraphNode>& toNodePtr : graph.m_Nodes)
        {
            if (!toNodePtr)
            {
                continue;
            }

            MaterialEdGraphNode& toNode = *toNodePtr;
            MaterialGraphNodeDef* toDef = toNode.GetNodeDef();
            if (!toDef)
            {
                continue;
            }

            for (int32_t inputIndex = 0; MaterialGraphNodeDef::Input* input = toDef->GetInput(inputIndex);
                 ++inputIndex)
            {
                if (!input->IsConnected())
                {
                    continue;
                }

                MaterialEdGraphNode* fromNode = graph.FindEdNodeByNodeDef(input->NodeDef);
                if (!fromNode)
                {
                    continue;
                }

                const Ed::PinId startPin = MaterialGraphIds::ToPinId(
                    fromNode,
                    Ed::PinKind::Output,
                    input->OutputIndex);
                const Ed::PinId endPin = MaterialGraphIds::ToPinId(
                    &toNode,
                    Ed::PinKind::Input,
                    inputIndex);
                const Ed::LinkId linkId = MaterialGraphIds::ToLinkId(
                    fromNode,
                    input->OutputIndex,
                    &toNode,
                    inputIndex);

                Ed::Link(linkId, startPin, endPin);
            }
        }
    }

    bool MaterialGraphWindow::TryConnectPins(
        Ed::PinId startPinId,
        Ed::PinId endPinId,
        Material& material,
        MaterialEdGraph& graph)
    {
        MaterialEdGraphNode* startNode = nullptr;
        MaterialEdGraphNode* endNode = nullptr;
        Ed::PinKind startKind = Ed::PinKind::Input;
        Ed::PinKind endKind = Ed::PinKind::Input;
        int32_t startIndex = 0;
        int32_t endIndex = 0;

        if (!MaterialGraphIds::FromPinId(startPinId, startNode, startKind, startIndex) ||
            !MaterialGraphIds::FromPinId(endPinId, endNode, endKind, endIndex))
        {
            return false;
        }

        MaterialEdGraphNode* fromNode = nullptr;
        MaterialEdGraphNode* toNode = nullptr;
        int32_t fromOutputIndex = 0;
        int32_t toInputIndex = 0;

        if (startKind == Ed::PinKind::Output && endKind == Ed::PinKind::Input)
        {
            fromNode = startNode;
            fromOutputIndex = startIndex;
            toNode = endNode;
            toInputIndex = endIndex;
        }
        else if (startKind == Ed::PinKind::Input && endKind == Ed::PinKind::Output)
        {
            fromNode = endNode;
            fromOutputIndex = endIndex;
            toNode = startNode;
            toInputIndex = startIndex;
        }
        else
        {
            return false;
        }

        std::string rejectReason;
        if (!graph.CanConnectPins(
                *fromNode,
                fromOutputIndex,
                *toNode,
                toInputIndex,
                material.m_ShadingModel,
                material.m_BlendMode,
                &rejectReason))
        {
            if (!rejectReason.empty())
            {
                ME_CORE_WARN("Material graph: rejected pin connection: {}", rejectReason);
            }
            return false;
        }

        return graph.ConnectPins(
            *fromNode,
            fromOutputIndex,
            *toNode,
            toInputIndex,
            material.m_ShadingModel,
            material.m_BlendMode);
    }

    void MaterialGraphWindow::HandleCreateLink(Material& material, MaterialEdGraph& graph)
    {
        // BeginCreate() may return false while still setting CreateItemAction::m_InActive;
        // EndCreate() must always run to pair with the internal Begin().
        if (!Ed::BeginCreate())
        {
            Ed::EndCreate();
            return;
        }

        Ed::PinId startPinId;
        Ed::PinId endPinId;
        if (Ed::QueryNewLink(&startPinId, &endPinId))
        {
            MaterialEditor* materialEditor = GetMaterialEditor(&m_Context);
        if (!materialEditor)
        {
            return;
        }
            if (TryConnectPins(startPinId, endPinId, material, graph))
            {
                if (Ed::AcceptNewItem())
                {
                    materialEditor->NotifyGraphChanged();
                }
            }
            else
            {
                Ed::RejectNewItem();
            }
        }

        Ed::EndCreate();
    }

    void MaterialGraphWindow::HandleDeleteLink(MaterialEdGraph& graph)
    {
        if (!Ed::BeginDelete())
        {
            return;
        }

        MaterialEditor* materialEditor = GetMaterialEditor(&m_Context);
        if (!materialEditor)
        {
            return;
        }

        Ed::LinkId linkId;
        Ed::PinId startPinId;
        Ed::PinId endPinId;
        while (Ed::QueryDeletedLink(&linkId, &startPinId, &endPinId))
        {
            MaterialEdGraphNode* fromNode = nullptr;
            MaterialEdGraphNode* toNode = nullptr;
            int32_t fromOutputIndex = 0;
            int32_t toInputIndex = 0;

            bool resolved = MaterialGraphIds::FromLinkId(
                linkId,
                fromNode,
                fromOutputIndex,
                toNode,
                toInputIndex);

            if (!resolved)
            {
                MaterialEdGraphNode* pinStartNode = nullptr;
                MaterialEdGraphNode* pinEndNode = nullptr;
                Ed::PinKind startKind = Ed::PinKind::Input;
                Ed::PinKind endKind = Ed::PinKind::Input;
                int32_t startIndex = 0;
                int32_t endIndex = 0;

                if (MaterialGraphIds::FromPinId(startPinId, pinStartNode, startKind, startIndex) &&
                    MaterialGraphIds::FromPinId(endPinId, pinEndNode, endKind, endIndex))
                {
                    if (startKind == Ed::PinKind::Output && endKind == Ed::PinKind::Input)
                    {
                        fromNode = pinStartNode;
                        fromOutputIndex = startIndex;
                        toNode = pinEndNode;
                        toInputIndex = endIndex;
                        resolved = true;
                    }
                    else if (startKind == Ed::PinKind::Input && endKind == Ed::PinKind::Output)
                    {
                        fromNode = pinEndNode;
                        fromOutputIndex = endIndex;
                        toNode = pinStartNode;
                        toInputIndex = startIndex;
                        resolved = true;
                    }
                }
            }

            if (resolved && toNode)
            {
                graph.DisconnectInput(*toNode, toInputIndex);
                if (Ed::AcceptDeletedItem())
                {
                    materialEditor->NotifyGraphChanged();
                }
            }
            else
            {
                Ed::RejectDeletedItem();
            }
        }

        Ed::NodeId deletedNodeId;
        while (Ed::QueryDeletedNode(&deletedNodeId))
        {
            MaterialEdGraphNode* nodeToDelete = MaterialGraphIds::FromNodeId(deletedNodeId);
            if (nodeToDelete == nullptr)
            {
                Ed::RejectDeletedItem();
                continue;
            }

            MaterialGraphNodeDef* nodeDef = nodeToDelete->GetNodeDef();
            if (nodeDef != nullptr && nodeDef->IsMaterialOutputNode())
            {
                Ed::RejectDeletedItem();
                continue;
            }

            if (materialEditor->GetSelectedEdNode() == nodeToDelete)
            {
                materialEditor->ClearSelectedEdNode();
            }

            if (graph.RemoveNode(*nodeToDelete) && Ed::AcceptDeletedItem())
            {
                MaterialGraphIds::Reset();
                materialEditor->NotifyGraphChanged();
                m_PushStoredPositionsToEditor = true;
            }
            else
            {
                Ed::RejectDeletedItem();
            }
        }

        Ed::EndDelete();
    }

    void MaterialGraphWindow::SyncNodePositions(MaterialEdGraph& graph)
    {
        for (const std::shared_ptr<MaterialEdGraphNode>& node : graph.m_Nodes)
        {
            if (!node)
            {
                continue;
            }

            const ImVec2 position = Ed::GetNodePosition(MaterialGraphIds::ToNodeId(node.get()));
            node->m_EditorPosX = position.x;
            node->m_EditorPosY = position.y;
        }
    }

    std::string_view MaterialGraphWindow::GetOwnerModuleId() const
    {
        return MaterialEditor::kModuleId;
    }
}
