#pragma once

#include "Viewport/EditorViewportClient.h"

namespace minEngine
{
    /** Material editor viewport: observes MaterialEditor::PreviewScene via base SceneViewport. */
    class MaterialEditorViewportClient : public EditorViewportClient
    {
    public:
        explicit MaterialEditorViewportClient(std::string debugName = "Material Editor Viewport");
        ~MaterialEditorViewportClient() override;

        void EndFrame() override;

        void SetupDefaultPreviewCamera();

    protected:
        void SyncRenderTargetSize() override;

    private:
        void SyncObservedPreviewScene();
    };
}
