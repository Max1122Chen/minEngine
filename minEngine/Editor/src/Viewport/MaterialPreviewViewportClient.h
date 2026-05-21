#pragma once

#include "Viewport/EditorViewportClient.h"
#include "Runtime/Function/Render/MaterialPreviewViewport.h"

namespace minEngine
{
    class RHI;

    /** Material preview panel: owned preview scene + second Submit (no Gizmo / pick / fly). */
    class MaterialPreviewViewportClient : public EditorViewportClient
    {
    public:
        explicit MaterialPreviewViewportClient(std::string debugName = "Material Preview Viewport");
        ~MaterialPreviewViewportClient() override;

        void EndFrame() override;

        void InitializePreviewViewport(RHI* rhi, uint32_t width, uint32_t height);
        void BuildPreviewContent();

        bool IsPreviewContentReady() const { return m_Preview.IsContentReady(); }

        MaterialPreviewViewport& GetMaterialPreviewViewport() { return m_Preview; }
        const MaterialPreviewViewport& GetMaterialPreviewViewport() const { return m_Preview; }

    protected:
        void SyncRenderTargetSize() override;

    private:
        void SyncPreviewCameraAspect();

        MaterialPreviewViewport m_Preview;
        bool m_PreviewInitialized = false;
    };
}
