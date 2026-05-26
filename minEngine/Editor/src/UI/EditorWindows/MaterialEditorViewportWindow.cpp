#include "UI/EditorWindows/MaterialEditorViewportWindow.h"

#include "Material/MaterialEditor.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/ViewportClientRegistry.h"

namespace minEngine
{
    void MaterialEditorViewportWindow::OnAttach()
    {
        EditorViewportWindow::OnAttach();

        if (MaterialEditor* materialEditor = GetMaterialEditorFromContext(&m_Context))
        {
            materialEditor->OnPreviewViewHostReady();
        }
    }

    EditorViewportClient& MaterialEditorViewportWindow::GetOrCreateViewportClient()
    {
        return m_Context.GetViewportRegistry().GetOrCreateMaterialEditorViewportClient(m_Id, m_Title);
    }

    const std::shared_ptr<RHITexture2D>& MaterialEditorViewportWindow::GetDisplayColorTexture() const
    {
        return GetMaterialEditorViewportClient().GetSceneViewport().GetColorTexture();
    }
}
