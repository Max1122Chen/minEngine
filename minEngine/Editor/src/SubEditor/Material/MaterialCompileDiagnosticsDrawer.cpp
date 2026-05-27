#include "MaterialCompileDiagnosticsDrawer.h"

#include "UI/Appearance/EditorAppearance.h"

#include "imgui.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"

namespace minEngine
{
    namespace
    {
        ImVec4 SeverityColor(const EditorAppearance& appearance, MaterialCompileDiagnostic::Severity level)
        {
            const EditorSemanticColors& colors = appearance.GetSemanticColors();
            switch (level)
            {
                case MaterialCompileDiagnostic::Info:
                    return appearance.GetDisplayColor(colors.DiagnosticInfo);
                case MaterialCompileDiagnostic::Warning:
                    return appearance.GetDisplayColor(colors.DiagnosticWarning);
                case MaterialCompileDiagnostic::Error:
                default:
                    return appearance.GetDisplayColor(colors.DiagnosticError);
            }
        }

        const char* SeverityLabel(MaterialCompileDiagnostic::Severity level)
        {
            switch (level)
            {
                case MaterialCompileDiagnostic::Info: return "Info";
                case MaterialCompileDiagnostic::Warning: return "Warn";
                case MaterialCompileDiagnostic::Error:
                default: return "Error";
            }
        }
    }

    void MaterialCompileDiagnosticsDrawer::Draw(const Material& material,
                                                const EditorAppearance& appearance,
                                                bool defaultOpen)
    {
        const ImGuiTreeNodeFlags headerFlags =
            defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

        if (!ImGui::CollapsingHeader("Compile Diagnostics", headerFlags))
        {
            return;
        }

        const std::vector<MaterialCompileDiagnostic>& diagnostics = material.m_LastCompileDiagnostics;
        if (diagnostics.empty())
        {
            ImGui::TextDisabled("No compile messages.");
            return;
        }

        for (size_t i = 0; i < diagnostics.size(); ++i)
        {
            const MaterialCompileDiagnostic& diag = diagnostics[static_cast<size_t>(i)];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextColored(SeverityColor(appearance, diag.Level), "[%s]", SeverityLabel(diag.Level));
            ImGui::SameLine();
            ImGui::TextWrapped("%s", diag.Message.c_str());
            ImGui::PopID();
        }
    }
}
