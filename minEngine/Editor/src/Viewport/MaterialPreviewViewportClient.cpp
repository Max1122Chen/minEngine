#include "MaterialPreviewViewportClient.h"

#include "Editor.h"
#include "EditorUIMode.h"
#include "Material/MaterialEditor.h"
#include "Material/MaterialEditorPreview.h"

#include "Runtime/Function/Render/MaterialPreviewViewport.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/SceneViewport.h"

namespace minEngine
{
    namespace
    {
        MaterialEditorPreview* ResolveMaterialPreview(Editor* editor)
        {
            if (!editor)
            {
                return nullptr;
            }

            MaterialEditorPreview& preview = editor->GetMaterialEditor().GetPreview();
            if (!preview.IsInitialized() || !preview.IsSceneReady())
            {
                return nullptr;
            }

            return &preview;
        }
    }

    MaterialPreviewViewportClient::MaterialPreviewViewportClient(std::string debugName)
        : EditorViewportClient(std::move(debugName))
    {
    }

    MaterialPreviewViewportClient::~MaterialPreviewViewportClient() = default;

    void MaterialPreviewViewportClient::EndFrame()
    {
        if (!m_Editor || m_Editor->GetUIMode() != EditorUIMode::MaterialEditing)
        {
            return;
        }

        MaterialEditorPreview* preview = ResolveMaterialPreview(m_Editor);
        if (!preview)
        {
            return;
        }

        MaterialPreviewViewport& previewViewport = preview->GetPreview();

        SyncRenderTargetSize();
        SyncPreviewCameraAspect();
        previewViewport.RefreshRenderScene();

        RHI* rhi = RenderSystem::Get().GetRHI();
        SceneViewport& viewport = preview->GetSceneViewport();
        viewport.ApplyPendingResize(rhi);

        const SceneDrawFlags flags = SceneDrawFlags::EnablePostProcess;
        const SceneDrawDesc desc = viewport.BuildDrawDesc(flags);
        if (desc.Scene && desc.Camera && desc.RenderTarget)
        {
            RenderSystem::Get().SubmitSceneDraw(desc);
        }
    }

    void MaterialPreviewViewportClient::SyncPreviewCameraAspect()
    {
        MaterialEditorPreview* preview = ResolveMaterialPreview(m_Editor);
        if (!preview)
        {
            return;
        }

        RenderCamera* camera = preview->GetSceneViewport().GetCamera();
        if (!camera)
        {
            return;
        }

        const Vector2 bufferSize = preview->GetSceneViewport().GetBufferSize();
        if (bufferSize.x > 0.0f && bufferSize.y > 0.0f)
        {
            camera->m_AspectRatio = bufferSize.x / bufferSize.y;
            camera->UpdateProjectionMatrix();
            camera->UpdateViewProjMatrix();
        }
    }

    void MaterialPreviewViewportClient::SyncRenderTargetSize()
    {
        MaterialEditorPreview* preview = ResolveMaterialPreview(m_Editor);
        if (!preview)
        {
            return;
        }

        SceneViewport& viewport = preview->GetSceneViewport();
        const Math::Vector2 bufferSize = viewport.GetBufferSize();

        const uint32_t requestedWidth = m_FrameState.ImageSize.x > 0.0f
            ? static_cast<uint32_t>(m_FrameState.ImageSize.x)
            : (bufferSize.x > 0.0f ? static_cast<uint32_t>(bufferSize.x) : 1u);
        const uint32_t requestedHeight = m_FrameState.ImageSize.y > 0.0f
            ? static_cast<uint32_t>(m_FrameState.ImageSize.y)
            : (bufferSize.y > 0.0f ? static_cast<uint32_t>(bufferSize.y) : 1u);

        if (m_LastRequestedWidth == 0 || m_LastRequestedHeight == 0)
        {
            if (bufferSize.x > 0.0f && bufferSize.y > 0.0f)
            {
                m_LastRequestedWidth = static_cast<uint32_t>(bufferSize.x);
                m_LastRequestedHeight = static_cast<uint32_t>(bufferSize.y);
            }
            else
            {
                m_LastRequestedWidth = requestedWidth;
                m_LastRequestedHeight = requestedHeight;
            }
            return;
        }

        if (requestedWidth == m_LastRequestedWidth && requestedHeight == m_LastRequestedHeight)
        {
            return;
        }

        const float targetWidthRatio = static_cast<float>(requestedWidth) / static_cast<float>(m_LastRequestedWidth);
        const float targetHeightRatio = static_cast<float>(requestedHeight) / static_cast<float>(m_LastRequestedHeight);

        viewport.RequestResizeByRatio(targetWidthRatio, targetHeightRatio);
        m_LastRequestedWidth = requestedWidth;
        m_LastRequestedHeight = requestedHeight;
    }
}
