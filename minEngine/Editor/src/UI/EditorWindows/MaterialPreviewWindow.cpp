#include "MaterialPreviewWindow.h"

#include "Shell/EditorContextHelpers.h"
#include "Shell/ViewportClientRegistry.h"

namespace minEngine
{
    void MaterialPreviewWindow::OnAttach()
    {
        EditorViewportWindow::OnAttach();
        if (MaterialEditor* materialEditor = GetMaterialEditor(&m_Context))
        {
            materialEditor->OnPreviewViewHostReady();
        }
    }

    EditorViewportClient& MaterialPreviewWindow::GetOrCreateViewportClient()
    {
        return m_Context.GetViewportRegistry().GetOrCreateMaterialPreviewViewportClient(m_Id, m_Title);
    }

    const std::shared_ptr<RHITexture2D>& MaterialPreviewWindow::GetDisplayColorTexture() const
    {
        MaterialEditor* materialEditor = GetMaterialEditor(&m_Context);
        static const std::shared_ptr<RHITexture2D> kEmpty;
        if (!materialEditor)
        {
            return kEmpty;
        }

        return materialEditor->GetPreview().GetSceneViewport().GetColorTexture();
    }
}
