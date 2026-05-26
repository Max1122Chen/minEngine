#pragma once

#include "Core.h"

#include <memory>

namespace minEngine
{
    class DirectionalLightComponent;
    class GameObject;
    class Material;
    class RenderScene;
    class Scene;
    class StaticMeshComponent;

    class StaticMesh;

    /** Thin preview world: Scene + preview mesh/light GOs. No SceneViewport or Submit. */
    class PreviewScene
    {
    public:
        static constexpr const char* kStaticMeshPreviewMaterialPath = "Materials/DefaultMaterial.memtl";

        PreviewScene() = default;
        ~PreviewScene();

        PreviewScene(const PreviewScene&) = delete;
        PreviewScene& operator=(const PreviewScene&) = delete;

        void BuildDefaultSphereScene();
        void ResetPreviewMeshToDefaultSphere();
        void SetPreviewMesh(const std::shared_ptr<StaticMesh>& mesh);
        void SetPreviewMaterial(const std::shared_ptr<Material>& material);
        bool EnsureStaticMeshPreviewMaterial();
        const std::shared_ptr<Material>& GetStaticMeshPreviewMaterial() const
        {
            return m_StaticMeshPreviewMaterial;
        }

        void RefreshRenderScene();
        void Shutdown();

        RenderScene* GetRenderScene();
        const RenderScene* GetRenderScene() const;

        bool IsContentReady() const { return m_ContentReady; }

    private:
        bool m_ContentReady = false;
        std::shared_ptr<Scene> m_Scene;
        std::shared_ptr<GameObject> m_PreviewMeshObject;
        std::shared_ptr<StaticMeshComponent> m_PreviewMeshComponent;
        std::shared_ptr<StaticMesh> m_DefaultSphereMesh;
        std::shared_ptr<Material> m_PreviewMaterial;
        std::shared_ptr<Material> m_StaticMeshPreviewMaterial;
        std::shared_ptr<GameObject> m_PreviewLightObject;
        std::shared_ptr<DirectionalLightComponent> m_PreviewLightComponent;
    };
}
