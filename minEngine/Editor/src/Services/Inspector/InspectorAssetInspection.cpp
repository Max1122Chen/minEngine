#include "Services/Inspector/InspectorAssetInspection.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    namespace
    {
        const std::shared_ptr<RHITexture2D> kEmptyTexture;
    }

    void InspectorAssetInspection::ClearInspectionTarget()
    {
        SetInspectionTarget(nullptr);
    }

    void InspectorAssetInspection::SetInspectionTarget(const AssetMeta* meta)
    {
        m_MaterialAsset.reset();
        m_DisplayKind = PreviewDisplayKind::None;

        if (meta == nullptr)
        {
            if (m_ViewportInitialized)
            {
                m_Viewport.SetObservedScene(nullptr);
            }

            m_World.Shutdown();
            ShutdownSceneViewport();
            return;
        }

        m_DisplayKind = ResolveDisplayKind(*meta);

        if (m_DisplayKind == PreviewDisplayKind::Scene3D)
        {
            RebuildScene3DPreview(*meta);
            return;
        }

        if (m_ViewportInitialized)
        {
            m_Viewport.SetObservedScene(nullptr);
        }

        m_World.Shutdown();
    }

    PreviewDisplayKind InspectorAssetInspection::ResolveDisplayKind(const AssetMeta& meta) const
    {
        if (meta.AssetType == "Material" || meta.AssetType == "StaticMesh")
        {
            return PreviewDisplayKind::Scene3D;
        }

        return PreviewDisplayKind::None;
    }

    bool InspectorAssetInspection::HasPreviewContent() const
    {
        return m_DisplayKind == PreviewDisplayKind::Scene3D && m_World.IsContentReady();
    }

    void InspectorAssetInspection::RebuildScene3DPreview(const AssetMeta& meta)
    {
        if (!m_World.IsContentReady())
        {
            m_World.BuildDefaultSphereScene();
        }

        if (!m_World.IsContentReady())
        {
            m_DisplayKind = PreviewDisplayKind::None;
            return;
        }

        if (meta.AssetType == "Material")
        {
            m_MaterialAsset = AssetManager::Get().LoadAsset<Material>(meta.AssetPath);
            if (!m_MaterialAsset)
            {
                ME_CORE_WARN("InspectorAssetInspection: failed to load material '{}'.", meta.AssetPath);
                m_DisplayKind = PreviewDisplayKind::None;
                return;
            }

            m_World.ResetPreviewMeshToDefaultSphere();
            m_World.SetPreviewMaterial(m_MaterialAsset);
            return;
        }

        if (meta.AssetType == "StaticMesh")
        {
            std::shared_ptr<StaticMesh> mesh = AssetManager::Get().LoadAsset<StaticMesh>(meta.AssetPath);
            if (!mesh)
            {
                ME_CORE_WARN(
                    "InspectorAssetInspection: failed to load static mesh '{}'.",
                    meta.AssetPath);
                m_DisplayKind = PreviewDisplayKind::None;
                return;
            }

            if (!m_World.EnsureStaticMeshPreviewMaterial())
            {
                ME_CORE_WARN(
                    "InspectorAssetInspection: static mesh preview material is not ready ('{}').",
                    PreviewScene::kStaticMeshPreviewMaterialPath);
                m_DisplayKind = PreviewDisplayKind::None;
                return;
            }

            m_World.SetPreviewMesh(mesh);
            m_World.SetPreviewMaterial(m_World.GetStaticMeshPreviewMaterial());
        }
    }

    void InspectorAssetInspection::EnsureSceneViewport(uint32_t squareSize)
    {
        const uint32_t clampedSize = squareSize > 0 ? squareSize : 1u;
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }

        if (!m_ViewportInitialized)
        {
            m_Viewport.Initialize(rhi, clampedSize, clampedSize);
            m_ViewportInitialized = true;
            m_ViewportWidth = clampedSize;
            m_ViewportHeight = clampedSize;
            SetupDefaultPreviewCamera();
            return;
        }

        if (m_ViewportWidth == clampedSize && m_ViewportHeight == clampedSize)
        {
            return;
        }

        if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
        {
            const float widthRatio = static_cast<float>(clampedSize) / static_cast<float>(m_ViewportWidth);
            const float heightRatio = static_cast<float>(clampedSize) / static_cast<float>(m_ViewportHeight);
            m_Viewport.RequestResizeByRatio(widthRatio, heightRatio);
        }

        m_ViewportWidth = clampedSize;
        m_ViewportHeight = clampedSize;
    }

    void InspectorAssetInspection::ShutdownSceneViewport()
    {
        if (!m_ViewportInitialized)
        {
            return;
        }

        m_Viewport.Shutdown();
        m_ViewportInitialized = false;
        m_ViewportWidth = 0;
        m_ViewportHeight = 0;
    }

    void InspectorAssetInspection::SetupDefaultPreviewCamera()
    {
        RenderCamera* camera = m_Viewport.GetCamera();
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

    void InspectorAssetInspection::SyncPreviewCameraAspect()
    {
        RenderCamera* camera = m_Viewport.GetCamera();
        if (!camera)
        {
            return;
        }

        const Vector2 bufferSize = m_Viewport.GetBufferSize();
        if (bufferSize.x > 0.0f && bufferSize.y > 0.0f)
        {
            camera->m_AspectRatio = bufferSize.x / bufferSize.y;
            camera->UpdateProjectionMatrix();
            camera->UpdateViewProjMatrix();
        }
    }

    void InspectorAssetInspection::RenderInspection(uint32_t squareSize)
    {
        if (m_DisplayKind != PreviewDisplayKind::Scene3D || !m_World.IsContentReady())
        {
            return;
        }

        EnsureSceneViewport(squareSize);
        if (!m_ViewportInitialized)
        {
            return;
        }

        m_Viewport.SetObservedScene(m_World.GetRenderScene());
        SyncPreviewCameraAspect();
        m_World.RefreshRenderScene();

        RHI* rhi = RenderSystem::Get().GetRHI();
        m_Viewport.ApplyPendingResize(rhi);

        const SceneDrawDesc desc = m_Viewport.BuildDrawDesc(SceneDrawFlags::EnablePostProcess);
        if (desc.Scene && desc.Camera && desc.RenderTarget)
        {
            RenderSystem::Get().SubmitSceneDraw(desc);
        }
    }

    const std::shared_ptr<RHITexture2D>& InspectorAssetInspection::GetSceneColorTexture() const
    {
        if (!m_ViewportInitialized)
        {
            return kEmptyTexture;
        }

        return m_Viewport.GetColorTexture();
    }

    void InspectorAssetInspection::Shutdown()
    {
        m_MaterialAsset.reset();
        m_DisplayKind = PreviewDisplayKind::None;
        m_World.Shutdown();
        ShutdownSceneViewport();
    }
}

