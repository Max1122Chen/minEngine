#include "SubEditor/Material/MaterialEditorInspectorSource.h"

#include "SubEditor/Material/MaterialEditor.h"
#include "Shell/IEditorContext.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include "SubEditor/Material/MaterialEditorSession.h"
#include "SubEditor/Material/MaterialGraphIds.h"
#include "SubEditor/Material/MaterialGraphNodeRegistry.h"
#include "SubEditor/Material/MaterialCompileDiagnosticsDrawer.h"
#include "SubEditor/Material/MaterialNodeDefPropertyDrawer.h"
#include "UI/Property/PropertyEditSession.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialEdGraphNode.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include <algorithm>

namespace minEngine
{
    namespace
    {
        constexpr float kDetailsFieldWidth = -1.0f;

        const char* ShadingModelLabel(MaterialShadingModel model)
        {
            switch (model)
            {
                case MaterialShadingModel::Unlit: return "Unlit";
                case MaterialShadingModel::BlinnPhong: return "BlinnPhong";
                case MaterialShadingModel::PBR: return "PBR";
                default: return "Unknown";
            }
        }

        const char* BlendModeLabel(MaterialBlendMode mode)
        {
            switch (mode)
            {
                case MaterialBlendMode::Opaque: return "Opaque";
                case MaterialBlendMode::Masked: return "Masked";
                case MaterialBlendMode::Translucent: return "Translucent";
                default: return "Unknown";
            }
        }

        void ComputeSpawnPosition(
            MaterialEdGraph& graph,
            MaterialEdGraphNode* selectedNode,
            float& outX,
            float& outY)
        {
            if (selectedNode)
            {
                outX = selectedNode->m_EditorPosX + 220.0f;
                outY = selectedNode->m_EditorPosY;
                return;
            }

            float maxX = 0.0f;
            float maxY = 0.0f;
            for (const std::shared_ptr<MaterialEdGraphNode>& node : graph.m_Nodes)
            {
                if (!node)
                {
                    continue;
                }

                maxX = std::max(maxX, node->m_EditorPosX);
                maxY = std::max(maxY, node->m_EditorPosY);
            }

            outX = maxX + 220.0f;
            outY = maxY;
        }
    }

    MaterialEditorInspectorSource::MaterialEditorInspectorSource(MaterialEditor& materialEditor)
        : m_MaterialEditor(materialEditor)
    {
    }

    bool MaterialEditorInspectorSource::HasInspectableSelection() const
    {
        return m_MaterialEditor.GetSession().HasOpenMaterial()
            || m_MaterialEditor.GetSelectedEdNode() != nullptr;
    }

    void MaterialEditorInspectorSource::DrawInspector()
    {
        IEditorContext* editorContext = m_MaterialEditor.GetEditorContext();
        if (editorContext == nullptr)
        {
            ImGui::Begin(kWindowTitle);
            ImGui::TextUnformatted("No material editor context.");
            ImGui::End();
            return;
        }

        if (!EditorWindowTypography::BeginPanel(*editorContext, kWindowTitle))
        {
            return;
        }

        EditorTypographyScope bodyTypography(editorContext->GetEditorAppearance(), EditorTypographyRole::Body);

        const ImVec2 scrollAvail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild(
            "MaterialDetailsScroll",
            scrollAvail,
            false,
            ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::PushItemWidth(kDetailsFieldWidth);

        const MaterialEditorSession& session = m_MaterialEditor.GetSession();
        if (!session.HasOpenMaterial())
        {
            ImGui::TextUnformatted("No material open.");
            ImGui::PopItemWidth();
            ImGui::EndChild();
            ImGui::End();
            return;
        }

        Material& material = *session.MaterialAsset;
        const MaterialShadingModel shadingModel = material.m_ShadingModel;
        const MaterialBlendMode blendMode = material.m_BlendMode;

        if (ImGui::BeginCombo("Shading Model", ShadingModelLabel(shadingModel)))
        {
            const MaterialShadingModel options[] = {
                MaterialShadingModel::Unlit,
                MaterialShadingModel::BlinnPhong,
                MaterialShadingModel::PBR,
            };

            for (MaterialShadingModel option : options)
            {
                const bool selected = (option == shadingModel);
                if (ImGui::Selectable(ShadingModelLabel(option), selected))
                {
                    m_MaterialEditor.SetShadingModel(option);
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("Blend Mode", BlendModeLabel(blendMode)))
        {
            const MaterialBlendMode options[] = {
                MaterialBlendMode::Opaque,
                MaterialBlendMode::Masked,
                MaterialBlendMode::Translucent,
            };

            for (MaterialBlendMode option : options)
            {
                const bool selected = (option == blendMode);
                if (ImGui::Selectable(BlendModeLabel(option), selected))
                {
                    m_MaterialEditor.SetBlendMode(option);
                }
            }
            ImGui::EndCombo();
        }

        MaterialEdGraph* graph = material.m_Graph.get();
        if (graph)
        {
            MaterialGraphNodeRegistry::EnsureRegistered();
            const std::vector<MaterialGraphNodeRegistryEntry>& creatableNodes =
                MaterialGraphNodeRegistry::GetCreatableNodes();

            const char* addPreview = "Add Node...";
            if (ImGui::BeginCombo("Add Node", addPreview))
            {
                int menuIndex = 0;
                for (const MaterialGraphNodeRegistryEntry& entry : creatableNodes)
                {
                    if (!entry.NodeDefClass)
                    {
                        continue;
                    }

                    ImGui::PushID(menuIndex++);
                    if (ImGui::Selectable(entry.DisplayName))
                    {
                        float spawnX = 0.0f;
                        float spawnY = 0.0f;
                        ComputeSpawnPosition(
                            *graph,
                            m_MaterialEditor.GetSelectedEdNode(),
                            spawnX,
                            spawnY);

                        MaterialEdGraphNode& newNode =
                            graph->AddNode(entry.NodeDefClass, spawnX, spawnY);
                        m_MaterialEditor.SetSelectedEdNode(&newNode);
                        MaterialGraphIds::Reset();
                        m_MaterialEditor.NotifyGraphChanged();
                        m_MaterialEditor.InvalidateGraphCanvas(false);
                    }
                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Asset");
        ImGui::TextWrapped("%s", session.AssetPath.c_str());

        MaterialEdGraphNode* selectedNode = m_MaterialEditor.GetSelectedEdNode();
        if (selectedNode && selectedNode->GetNodeDef())
        {
            MaterialGraphNodeDef* nodeDef = selectedNode->GetNodeDef();
            const MaterialGraphNodeStyle style = MaterialGraphNodeRegistry::GetStyle(nodeDef);

            ImGui::Separator();
            ImGui::TextUnformatted("Selected Node");
            ImGui::TextColored(ImColor(style.HeaderColor), "%s", style.DisplayName);

            PropertyEditSession nodeDefEditSession = PropertyEditSession::ForAssetDefaults();
            nodeDefEditSession.OnMarkDirty = [this]() { m_MaterialEditor.NotifyGraphChanged(); };
            MaterialNodeDefPropertyDrawer::DrawProperties(nodeDef, m_MaterialEditor, nodeDefEditSession);
        }
        else
        {
            ImGui::Separator();
            ImGui::TextDisabled("Select a node in the graph to edit its parameters.");
        }

        ImGui::Separator();
        if (IEditorContext* editorContext = m_MaterialEditor.GetEditorContext())
        {
            MaterialCompileDiagnosticsDrawer::Draw(material, editorContext->GetEditorAppearance(), true);
        }

        ImGui::PopItemWidth();
        ImGui::EndChild();
        ImGui::End();
    }
}
