#include "MaterialCompileDiagnosticsDrawer.h"

#include "imgui.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"

namespace minEngine
{
    namespace
    {
        ImVec4 SeverityColor(MaterialCompileDiagnostic::Severity level)
        {
            switch (level)
            {
                case MaterialCompileDiagnostic::Info:
                    return ImVec4(0.55f, 0.75f, 0.95f, 1.0f);
                case MaterialCompileDiagnostic::Warning:
                    return ImVec4(0.95f, 0.78f, 0.35f, 1.0f);
                case MaterialCompileDiagnostic::Error:
                default:
                    return ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
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

    void MaterialCompileDiagnosticsDrawer::Draw(const Material& material, bool defaultOpen)
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
            ImGui::TextColored(SeverityColor(diag.Level), "[%s]", SeverityLabel(diag.Level));
            ImGui::SameLine();
            ImGui::TextWrapped("%s", diag.Message.c_str());
            ImGui::PopID();
        }
    }
}
