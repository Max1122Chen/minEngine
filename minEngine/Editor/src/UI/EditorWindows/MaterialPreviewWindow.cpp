#include "MaterialPreviewWindow.h"

#include "Editor.h"
#include "Material/MaterialEditor.h"

namespace minEngine
{
    void MaterialPreviewWindow::OnAttach()
    {
        EditorViewportWindow::OnAttach();
        m_Editor.GetMaterialEditor().OnPreviewViewHostReady();
    }

    EditorViewportClient& MaterialPreviewWindow::GetOrCreateViewportClient()
    {
        return m_Editor.GetOrCreateMaterialPreviewViewportClient(m_Id, m_Title);
    }

    const std::shared_ptr<RHITexture2D>& MaterialPreviewWindow::GetDisplayColorTexture() const
    {
        return m_Editor.GetMaterialEditor()
            .GetPreview()
            .GetSceneViewport()
            .GetColorTexture();
    }
}
