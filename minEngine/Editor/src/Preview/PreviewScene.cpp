#include "Preview/PreviewScene.h"

#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

namespace minEngine
{
    namespace PreviewAssetGuids
    {
        // TODO: Try to find the sphere mesh in the Assets directory instead of using hardcoded GUID
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
                    "PreviewScene: failed to load {} (GUID {}). Error: {}",
                    label,
                    guid.ToString(),
                    errorMessage.empty() ? "unknown" : errorMessage);
                return nullptr;
            }

            std::shared_ptr<T> typedAsset = std::dynamic_pointer_cast<T>(asset);
            if (!typedAsset)
            {
                ME_CORE_WARN(
                    "PreviewScene: asset for {} (GUID {}) is not the expected type.",
                    label,
                    guid.ToString());
                return nullptr;
            }

            const AssetMeta* meta = AssetManager::Get().FindAssetMetaByGuid(guid);
            const std::string assetPath = meta ? meta->AssetPath : std::string("<unknown>");
            ME_CORE_INFO(
                "PreviewScene: loaded {} from '{}' (GUID {}).",
                label,
                assetPath,
                guid.ToString());
            return typedAsset;
        }
    }

    PreviewScene::~PreviewScene()
    {
        Shutdown();
    }

    void PreviewScene::BuildDefaultSphereScene()
    {
        if (m_ContentReady)
        {
            return;
        }

        m_Scene = NewObject<Scene>();
        m_Scene->m_SceneName = "EditorPreview";
        m_Scene->EnsureRenderScene();
        RenderScene* renderScene = m_Scene->GetRenderScene();

        std::shared_ptr<StaticMesh> previewMesh = LoadPreviewAssetByGuid<StaticMesh>(
            PreviewAssetGuids::kEngineDefaultSphereMesh,
            "preview mesh (EngineDefault sphere)");

        if (!previewMesh)
        {
            m_Scene.reset();
            if (ObjectManager::HasInstance())
            {
                ObjectManager::Get().CollectGarbage();
            }

            ME_CORE_ERROR(
                "PreviewScene: preview mesh GUID not in registry. "
                "Ensure Editor scanned EngineDefault assets (EngineConfig EngineDefaultAssetsRoot, sphere.obj).");
            return;
        }

        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().RegisterGarbageRootSource(this, [this](const ObjectReachabilityMarker& markReachable) {
                if (m_Scene)
                {
                    m_Scene->MarkReachableObjects(markReachable);
                }
            });
        }

        m_PreviewMeshObject = m_Scene->CreateGameObject();
        m_PreviewMeshComponent = m_PreviewMeshObject->AddComponent<StaticMeshComponent>();
        m_PreviewMeshObject->SetRootComponent(m_PreviewMeshComponent.get());
        m_DefaultSphereMesh = previewMesh;
        m_PreviewMeshComponent->SetMesh(m_DefaultSphereMesh);
        m_PreviewMeshObject->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        m_PreviewMeshObject->SetScale(Vector3(1.0f, 1.0f, 1.0f));

        m_PreviewLightObject = m_Scene->CreateGameObject();
        m_PreviewLightComponent = m_PreviewLightObject->AddComponent<DirectionalLightComponent>();
        m_PreviewLightObject->SetRootComponent(m_PreviewLightComponent.get());
        m_PreviewLightComponent->SetLightColor(Vector4(1.0f, 0.98f, 0.95f, 1.0f));
        m_PreviewLightComponent->SetIntensity(1.2f);
        m_PreviewLightComponent->SetDiffuseFactor(12.0f);
        m_PreviewLightObject->SetRotation(Vector3(-52.0f, 132.0f, 0.0f));

        RefreshRenderScene();

        m_ContentReady = true;
        ME_CORE_INFO(
            "PreviewScene: preview world ready (mesh proxies={}, dir lights={}).",
            renderScene->m_PrimitiveSceneProxies.size(),
            renderScene->m_DirectionalLightSceneProxies.size());

        if (m_PreviewMaterial)
        {
            SetPreviewMaterial(m_PreviewMaterial);
        }
    }

    void PreviewScene::ResetPreviewMeshToDefaultSphere()
    {
        if (!m_ContentReady || !m_PreviewMeshComponent)
        {
            return;
        }

        if (!m_DefaultSphereMesh)
        {
            m_DefaultSphereMesh = LoadPreviewAssetByGuid<StaticMesh>(
                PreviewAssetGuids::kEngineDefaultSphereMesh,
                "preview mesh (EngineDefault sphere)");
        }

        if (!m_DefaultSphereMesh)
        {
            ME_CORE_WARN("PreviewScene: default sphere mesh is not available.");
            return;
        }

        m_PreviewMeshComponent->SetMesh(m_DefaultSphereMesh);
        RefreshRenderScene();
    }

    void PreviewScene::SetPreviewMesh(const std::shared_ptr<StaticMesh>& mesh)
    {
        if (!mesh)
        {
            return;
        }

        if (!m_ContentReady)
        {
            BuildDefaultSphereScene();
        }

        if (!m_ContentReady || !m_PreviewMeshComponent)
        {
            ME_CORE_WARN("PreviewScene: cannot set preview mesh before preview world is ready.");
            return;
        }

        m_PreviewMeshComponent->SetMesh(mesh);
        RefreshRenderScene();

        ME_CORE_INFO("PreviewScene: preview mesh set.");
    }

    bool PreviewScene::EnsureStaticMeshPreviewMaterial()
    {
        if (m_StaticMeshPreviewMaterial)
        {
            return true;
        }

        m_StaticMeshPreviewMaterial =
            AssetManager::Get().LoadAsset<Material>(kStaticMeshPreviewMaterialPath);
        if (!m_StaticMeshPreviewMaterial)
        {
            ME_CORE_WARN(
                "PreviewScene: failed to load static mesh preview material '{}'.",
                kStaticMeshPreviewMaterialPath);
            return false;
        }

        if (!m_StaticMeshPreviewMaterial->IsCompiledForDraw())
        {
            if (!m_StaticMeshPreviewMaterial->Compile())
            {
                ME_CORE_WARN(
                    "PreviewScene: static mesh preview material compile failed ('{}').",
                    kStaticMeshPreviewMaterialPath);
            }
        }

        return m_StaticMeshPreviewMaterial->IsCompiledForDraw();
    }

    void PreviewScene::SetPreviewMaterial(const std::shared_ptr<Material>& material)
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
                ME_CORE_WARN("PreviewScene: preview material compile failed.");
            }
        }

        if (!material->IsCompiledForDraw())
        {
            ME_CORE_WARN("PreviewScene: preview material is not ready for draw.");
        }

        m_PreviewMeshComponent->SetMaterial(material);
        RefreshRenderScene();

        ME_CORE_INFO(
            "PreviewScene: preview material set (compiled={}).",
            material->IsCompiledForDraw());
    }

    void PreviewScene::RefreshRenderScene()
    {
        if (!m_Scene || !m_PreviewMeshComponent || !m_PreviewLightComponent)
        {
            return;
        }

        RenderScene* renderScene = m_Scene->GetRenderScene();
        if (!renderScene)
        {
            return;
        }

        renderScene->UpdatePrimitive(m_PreviewMeshComponent.get());
        renderScene->UpdateLight(m_PreviewLightComponent.get());
    }

    void PreviewScene::Shutdown()
    {
        if (!m_Scene && !m_ContentReady)
        {
            return;
        }

        m_ContentReady = false;
        m_PreviewLightComponent.reset();
        m_PreviewLightObject.reset();
        m_PreviewMaterial.reset();
        m_StaticMeshPreviewMaterial.reset();
        m_DefaultSphereMesh.reset();
        m_PreviewMeshComponent.reset();
        m_PreviewMeshObject.reset();

        if (m_Scene)
        {
            m_Scene.reset();
            if (ObjectManager::HasInstance())
            {
                ObjectManager::Get().CollectGarbage();
            }
        }

        if (ObjectManager::HasInstance())
        {
            ObjectManager::Get().UnregisterGarbageRootSource(this);
        }
    }

    RenderScene* PreviewScene::GetRenderScene()
    {
        return m_Scene ? m_Scene->GetRenderScene() : nullptr;
    }

    const RenderScene* PreviewScene::GetRenderScene() const
    {
        return m_Scene ? m_Scene->GetRenderScene() : nullptr;
    }
}
