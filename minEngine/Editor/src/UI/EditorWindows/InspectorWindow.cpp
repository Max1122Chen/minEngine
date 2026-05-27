#include "UI/EditorWindows/InspectorWindow.h"

#include "Services/AssetWorkflowModule.h"
#include "Shell/EditorSubModule.h"
#include "Shell/IEditorInspectorSource.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

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

        if (!source)
        {
            if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str()))
            {
                return;
            }

            EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);
            ImGui::TextUnformatted("No inspector for the active context.");
            ImGui::End();
            return;
        }

        if (!source->HasInspectableSelection())
        {
            if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str()))
            {
                return;
            }

            EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);
            ImGui::TextUnformatted("Nothing selected.");
            ImGui::End();
            return;
        }

        source->DrawInspector();
    }
}
