#include "UI/EditorWindows/InspectorWindow.h"

#include "Shell/IEditorInspectorSource.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include "imgui.h"

namespace minEngine
{
    void InspectorWindow::OnDraw()
    {
        EditorSubModule* active = m_Context.GetActiveSubModule();
        if (!active)
        {
            if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str()))
            {
                return;
            }

            EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);
            ImGui::TextUnformatted("No active editor module.");
            ImGui::End();
            return;
        }

        IEditorInspectorSource* source = active->GetInspectorSource();
        if (!source)
        {
            if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str()))
            {
                return;
            }

            EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);
            ImGui::TextUnformatted("No inspector for the active module.");
            ImGui::End();
            return;
        }

        source->DrawInspector();
    }
}
