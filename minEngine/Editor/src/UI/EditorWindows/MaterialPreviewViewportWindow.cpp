#include "MaterialPreviewViewportWindow.h"

#include "Editor.h"

#include "imgui.h"

namespace minEngine
{
    EditorViewportClient& MaterialPreviewViewportWindow::GetOrCreateViewportClient()
    {
        return m_Editor.GetOrCreateMaterialPreviewViewportClient(m_Id, m_Title);
    }

    const std::shared_ptr<RHITexture2D>& MaterialPreviewViewportWindow::GetDisplayColorTexture() const
    {
        return GetMaterialPreviewViewportClient()
            .GetMaterialPreviewViewport()
            .GetSceneViewport()
            .GetColorTexture();
    }

    void MaterialPreviewViewportWindow::OnDrawViewportOverlay(EditorViewportClient& /*client*/,
                                                            const ViewportFrameState& /*frameState*/)
    {
        ImGui::TextUnformatted("Material preview (P5 debug)");
        ImGui::TextUnformatted("Independent RenderScene + RT; shadows off.");
    }
}
