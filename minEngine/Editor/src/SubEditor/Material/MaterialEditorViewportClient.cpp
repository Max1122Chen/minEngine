#include "SubEditor/Material/MaterialEditorViewportClient.h"

#include "SubEditor/Material/MaterialEditor.h"
#include "Preview/PreviewScene.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/EditorSubModule.h"

#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"

#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    namespace
    {
        MaterialEditor* ResolveMaterialEditor(IEditorContext* context)
        {
            return GetMaterialEditorFromContext(context);
        }

        bool IsMaterialEditingMode(IEditorContext* context)
        {
            const EditorSubModule* active = context ? context->GetActiveSubModule() : nullptr;
            return active && active->GetModuleId() == MaterialEditor::kModuleId;
        }
    }

    MaterialEditorViewportClient::MaterialEditorViewportClient(std::string debugName)
        : EditorViewportClient(std::move(debugName))
    {
        ViewportAspectPolicy aspectPolicy;
        aspectPolicy.bKeepAspect = true;
        aspectPolicy.TargetAspect = 1.0f;
        aspectPolicy.FitMode = ViewportImageFitMode::Contain;
        SetAspectPolicy(aspectPolicy);
    }

    MaterialEditorViewportClient::~MaterialEditorViewportClient() = default;

    void MaterialEditorViewportClient::SetupDefaultPreviewCamera()
    {
        RenderCamera* camera = GetSceneViewport().GetCamera();
        if (!camera)
        {
            return;
        }

        const Vector3 eye(1.15f, 0.8f, 1.15f);
        const Vector3 target(0.0f, 0.0f, 0.0f);
        const Vector3 up(0.0f, 1.0f, 0.0f);

        camera->m_Position = eye;
        camera->m_zNear = 0.1f;
        camera->m_zFar = 200.0f;
        camera->m_FOV = 45.0f;
        camera->m_ViewMatrix = glm::lookAt(eye, target, up);
        camera->UpdateProjectionMatrix();
        camera->UpdateViewProjMatrix();
    }

    void MaterialEditorViewportClient::SyncObservedPreviewScene()
    {
        MaterialEditor* materialEditor = ResolveMaterialEditor(m_Context);
        if (!materialEditor || !materialEditor->GetPreviewScene().IsContentReady())
        {
            GetSceneViewport().SetObservedScene(nullptr);
            return;
        }

        GetSceneViewport().SetObservedScene(materialEditor->GetPreviewScene().GetRenderScene());
    }

    void MaterialEditorViewportClient::EndFrame()
    {
        if (!IsMaterialEditingMode(m_Context))
        {
            return;
        }

        MaterialEditor* materialEditor = ResolveMaterialEditor(m_Context);
        if (!materialEditor)
        {
            return;
        }

        if (!IsSceneViewportInitialized())
        {
            if (RHI* rhi = RenderSystem::Get().GetRHI())
            {
                InitializeEditorSceneViewport(rhi, 512, 512);
                SetupDefaultPreviewCamera();
            }
        }

        if (!IsSceneViewportInitialized())
        {
            return;
        }

        SyncRenderTargetSize();
        SyncObservedPreviewScene();
        SyncSceneViewportCameraAspect();

        materialEditor->GetPreviewScene().RefreshRenderScene();

        const SceneDrawFlags flags = SceneDrawFlags::EnableSkyBox | SceneDrawFlags::EnablePostProcess;
        SubmitObservedScene(flags);
    }

    void MaterialEditorViewportClient::SyncRenderTargetSize()
    {
        EditorViewportClient::SyncRenderTargetSize();
    }
}
