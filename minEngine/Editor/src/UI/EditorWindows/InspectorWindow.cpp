#include "UI/EditorWindows/InspectorWindow.h"

#include "Shell/IEditorInspectorSource.h"

#include "imgui.h"

namespace minEngine
{
    void InspectorWindow::OnDraw()
    {
        EditorSubModule* active = m_Context.GetActiveSubModule();
        if (!active)
        {
            ImGui::Begin(m_Title.c_str());
            ImGui::TextUnformatted("No active editor module.");
            ImGui::End();
            return;
        }

        IEditorInspectorSource* source = active->GetInspectorSource();
        if (!source)
        {
            ImGui::Begin(m_Title.c_str());
            ImGui::TextUnformatted("No inspector for the active module.");
            ImGui::End();
            return;
        }

        source->DrawInspector();
    }
}
