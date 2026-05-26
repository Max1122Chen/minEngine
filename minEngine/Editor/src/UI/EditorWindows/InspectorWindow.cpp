#include "UI/EditorWindows/InspectorWindow.h"

#include "Services/AssetWorkflowModule.h"
#include "Shell/EditorSubModule.h"
#include "Shell/IEditorInspectorSource.h"

#include "imgui.h"

namespace minEngine
{
    void InspectorWindow::OnDraw()
    {
        IEditorInspectorSource* source = nullptr;

        AssetWorkflowModule& assetWorkflow = m_Context.GetAssetWorkflow();
        if (assetWorkflow.IsContentBrowserInspectorActive()
            && assetWorkflow.GetInspectorSource()->HasInspectableSelection())
        {
            source = assetWorkflow.GetInspectorSource();
        }
        else if (EditorSubModule* active = m_Context.GetActiveSubModule())
        {
            source = active->GetInspectorSource();
        }

        ImGui::Begin(m_Title.c_str());
        if (!source)
        {
            ImGui::TextUnformatted("No inspector for the active context.");
            ImGui::End();
            return;
        }

        if (!source->HasInspectableSelection())
        {
            ImGui::TextUnformatted("Nothing selected.");
            ImGui::End();
            return;
        }

        source->DrawInspector();
        ImGui::End();
    }
}
