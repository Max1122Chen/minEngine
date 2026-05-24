#include "MaterialPreviewViewport.h"

#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    namespace PreviewAssetGuids
    {
        // EngineDefault/Meshes/BasicShapes/sphere.obj.meta
        const GUID kEngineDefaultSphereMesh{ 607701751770204618ULL, 9254168834165808632ULL };

    }

    namespace
    {
        template<typename T>
        std::shared_ptr<T> LoadPreviewAssetByGuid(const GUID& guid, const char* label)
        {
            std::string errorMessage;
            std::shared_ptr<Asset> asset = AssetManager::Get().LoadAssetByGUID(guid, errorMessage);
            if (!asset)
            {
                ME_CORE_WARN(
                    "MaterialPreviewViewport: failed to load {} (GUID {}). Error: {}",
                    label,
                    guid.ToString(),
                    errorMessage.empty() ? "unknown" : errorMessage);
                return nullptr;
            }

            std::shared_ptr<T> typedAsset = std::dynamic_pointer_cast<T>(asset);
            if (!typedAsset)
            {
                ME_CORE_WARN(
                    "MaterialPreviewViewport: asset for {} (GUID {}) is not the expected type.",
                    label,
                    guid.ToString());
                return nullptr;
            }

            const AssetMeta* meta = AssetManager::Get().FindAssetMetaByGuid(guid);
            const std::string assetPath = meta ? meta->AssetPath : std::string("<unknown>");
            ME_CORE_INFO(
                "MaterialPreviewViewport: loaded {} from '{}' (GUID {}).",
                label,
                assetPath,
                guid.ToString());
            return typedAsset;
        }
    }

    void MaterialPreviewViewport::Initialize(RHI* rhi, uint32_t width, uint32_t height)
    {
        if (m_Initialized || !rhi)
        {
            return;
        }

        m_Viewport.Initialize(rhi, width, height);
        SetupPreviewCamera();
        SyncPreviewCameraAspect();

        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().RegisterGarbageRootSource(this, [this](const ObjectReachabilityMarker& markReachable) {
                if (m_PreviewScene)
                {
                    m_PreviewScene->MarkReachableObjects(markReachable);
                }
            });
        }

        m_Initialized = true;
    }

    void MaterialPreviewViewport::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        m_ContentReady = false;
        m_PreviewLightComponent.reset();
        m_PreviewLightObject.reset();
        m_PreviewMaterial.reset();
        m_PreviewMeshComponent.reset();
        m_PreviewMeshObject.reset();

        if (m_PreviewScene)
        {
            m_PreviewScene.reset();
            if (ObjectManager::HasInstance())
            {
                ObjectManager::Get().CollectGarbage();
            }
        }

        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().UnregisterGarbageRootSource(this);
        }

        m_Viewport.Shutdown();
        m_Initialized = false;
    }

    void MaterialPreviewViewport::BuildPreviewScene()
    {
        if (m_ContentReady || !m_Initialized)
        {
            return;
        }

        m_PreviewScene = NewObject<Scene>();
        m_PreviewScene->m_SceneName = "MaterialPreview";
        m_PreviewScene->EnsureRenderScene();
        RenderScene* renderScene = m_PreviewScene->GetRenderScene();
        m_Viewport.SetObservedScene(renderScene);

        std::shared_ptr<StaticMesh> previewMesh = LoadPreviewAssetByGuid<StaticMesh>(
            PreviewAssetGuids::kEngineDefaultSphereMesh,
            "preview mesh (EngineDefault sphere)");

        if (!previewMesh)
        {
            if (m_PreviewScene)
            {
                m_PreviewScene.reset();
                if (ObjectManager::HasInstance())
                {
                    ObjectManager::Get().CollectGarbage();
                }
            }

            m_Viewport.SetObservedScene(nullptr);
            ME_CORE_ERROR(
                "MaterialPreviewViewport: preview mesh GUID not in registry. "
                "Ensure Editor scanned EngineDefault assets (EngineConfig EngineDefaultAssetsRoot, sphere.obj).");
            return;
        }

        m_PreviewMeshObject = m_PreviewScene->CreateGameObject();
        m_PreviewMeshComponent = m_PreviewMeshObject->AddComponent<StaticMeshComponent>();
        m_PreviewMeshObject->SetRootComponent(m_PreviewMeshComponent.get());
        m_PreviewMeshComponent->SetMesh(previewMesh);
        m_PreviewMeshObject->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        m_PreviewMeshObject->SetScale(Vector3(1.0f, 1.0f, 1.0f));

        m_PreviewLightObject = m_PreviewScene->CreateGameObject();
        m_PreviewLightComponent = m_PreviewLightObject->AddComponent<DirectionalLightComponent>();
        m_PreviewLightObject->SetRootComponent(m_PreviewLightComponent.get());
        m_PreviewLightComponent->SetLightColor(Vector4(1.0f, 0.98f, 0.95f, 1.0f));
        m_PreviewLightComponent->SetIntensity(1.2f);
        m_PreviewLightComponent->SetDiffuseFactor(12.0f);
        // Key light from camera side (+X,+Z) and above — avoids pure top-down on the front face.
        m_PreviewLightObject->SetRotation(Vector3(-52.0f, 132.0f, 0.0f));

        SetupPreviewCamera();
        RefreshRenderScene();

        m_ContentReady = true;
        ME_CORE_INFO(
            "MaterialPreviewViewport: preview scene ready (mesh proxies={}, dir lights={}).",
            renderScene->m_PrimitiveSceneProxies.size(),
            renderScene->m_DirectionalLightSceneProxies.size());

        if (m_PreviewMaterial)
        {
            SetPreviewMaterial(m_PreviewMaterial);
        }
    }

    void MaterialPreviewViewport::SetPreviewMaterial(const std::shared_ptr<Material>& material)
    {
        if (!m_ContentReady || !m_PreviewMeshComponent)
        {
            m_PreviewMaterial = material;
            return;
        }

        m_PreviewMaterial = material;
        if (!material)
        {
            m_PreviewMeshComponent->SetMaterial(nullptr);
            RefreshRenderScene();
            return;
        }

        if (!material->IsCompiledForDraw())
        {
            if (!material->Compile())
            {
                ME_CORE_WARN("MaterialPreviewViewport: preview material compile failed.");
            }
        }

        if (!material->IsCompiledForDraw())
        {
            ME_CORE_WARN("MaterialPreviewViewport: preview material is not ready for draw.");
        }

        m_PreviewMeshComponent->SetMaterial(material);
        RefreshRenderScene();

        ME_CORE_INFO(
            "MaterialPreviewViewport: preview material set (compiled={}).",
            material->IsCompiledForDraw());
    }

    void MaterialPreviewViewport::SetupPreviewCamera()
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

    void MaterialPreviewViewport::RefreshRenderScene()
    {
        if (!m_PreviewScene || !m_PreviewMeshComponent || !m_PreviewLightComponent)
        {
            return;
        }

        RenderScene* renderScene = m_PreviewScene->GetRenderScene();
        if (!renderScene)
        {
            return;
        }

        renderScene->UpdatePrimitive(m_PreviewMeshComponent.get());
        renderScene->UpdateLight(m_PreviewLightComponent.get());
    }

    void MaterialPreviewViewport::SyncPreviewCameraAspect()
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
}
