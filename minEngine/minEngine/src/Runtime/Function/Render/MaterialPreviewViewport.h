#pragma once

#include "Core.h"
#include "SceneViewport.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Scene;
    class GameObject;
    class DirectionalLightComponent;
    class Material;
    class StaticMeshComponent;
    class RHI;

    /** Standalone preview world: sphere mesh + directional light + owned SceneViewport (P5). */
    class MaterialPreviewViewport
    {
    public:
        /** RT + camera only; safe before AssetManager ScanAssets. */
        void Initialize(RHI* rhi, uint32_t width, uint32_t height);
        void Shutdown();

        /** Sphere mesh + lights; call after Editor asset registry is ready. */
        void BuildPreviewScene();

        /** Apply session material to preview mesh (no-op until BuildPreviewScene). */
        void SetPreviewMaterial(const std::shared_ptr<Material>& material);

        SceneViewport& GetSceneViewport() { return m_Viewport; }
        const SceneViewport& GetSceneViewport() const { return m_Viewport; }

        bool IsInitialized() const { return m_Initialized; }
        bool IsContentReady() const { return m_ContentReady; }

        void RefreshRenderScene();
        void SetupPreviewCamera();

    private:
        void SyncPreviewCameraAspect();

        bool m_Initialized = false;
        bool m_ContentReady = false;
        SceneViewport m_Viewport;
        std::shared_ptr<Scene> m_PreviewScene;
        std::shared_ptr<GameObject> m_PreviewMeshObject;
        std::shared_ptr<StaticMeshComponent> m_PreviewMeshComponent;
        std::shared_ptr<Material> m_PreviewMaterial;
        std::shared_ptr<GameObject> m_PreviewLightObject;
        std::shared_ptr<DirectionalLightComponent> m_PreviewLightComponent;
    };
}
