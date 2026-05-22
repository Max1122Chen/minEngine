#include "MaterialDetailsWindow.h"

#include "imgui.h"

#include "Editor.h"
#include "Material/MaterialEditor.h"
#include "Material/MaterialEditorSession.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"

namespace minEngine
{
    namespace
    {
        const char* ShadingModelLabel(MaterialShadingModel model)
        {
            switch (model)
            {
                case MaterialShadingModel::Unlit: return "Unlit";
                case MaterialShadingModel::BlinnPhong: return "BlinnPhong";
                default: return "Unknown";
            }
        }
    }

    void MaterialDetailsWindow::OnDraw()
    {
        ImGui::Begin(m_Title.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        MaterialEditor& materialEditor = m_Editor.GetMaterialEditor();
        const MaterialEditorSession& session = materialEditor.GetSession();
        if (!session.HasOpenMaterial())
        {
            ImGui::TextUnformatted("No material open.");
            ImGui::End();
            return;
        }

        Material& material = *session.MaterialAsset;
        const MaterialShadingModel shadingModel = material.m_ShadingModel;

        if (ImGui::BeginCombo("Shading Model", ShadingModelLabel(shadingModel)))
        {
            const MaterialShadingModel options[] = {
                MaterialShadingModel::Unlit,
                MaterialShadingModel::BlinnPhong,
            };

            for (MaterialShadingModel option : options)
            {
                const bool selected = (option == shadingModel);
                if (ImGui::Selectable(ShadingModelLabel(option), selected))
                {
                    materialEditor.SetShadingModel(option);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Asset");
        ImGui::TextWrapped("%s", session.AssetPath.c_str());

        if (!material.m_LastCompileDiagnostics.empty())
        {
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Compile diagnostics", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (const MaterialCompileDiagnostic& diag : material.m_LastCompileDiagnostics)
                {
                    ImGui::BulletText("%s", diag.Message.c_str());
                }
            }
        }

        ImGui::End();
    }
}
