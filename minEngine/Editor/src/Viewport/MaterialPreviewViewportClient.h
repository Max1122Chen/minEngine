#pragma once

#include "Viewport/EditorViewportClient.h"

namespace minEngine
{
    /** Material preview panel: RT resize + Submit only (preview world owned by MaterialEditor). */
    class MaterialPreviewViewportClient : public EditorViewportClient
    {
    public:
        explicit MaterialPreviewViewportClient(std::string debugName = "Material Preview Viewport");
        ~MaterialPreviewViewportClient() override;

        void EndFrame() override;

        void SetViewportPanelId(std::string panelId) { m_ViewportPanelId = std::move(panelId); }
        const std::string& GetViewportPanelId() const { return m_ViewportPanelId; }

    protected:
        void SyncRenderTargetSize() override;

    private:
        void SyncPreviewCameraAspect();

        std::string m_ViewportPanelId;
    };
}
