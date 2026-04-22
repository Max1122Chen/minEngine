#include "RenderScene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"   // TODO: maybe remove this include later
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"

#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/SpotLightComponent.h"
#include "Runtime/Function/Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/SpotLightSceneProxy.h"
#include <algorithm>


namespace minEngine
{
    RenderScene::~RenderScene() = default;

    void RenderScene::UpdatePrimitive(PrimitiveComponent *primitiveComponent)
    {
        if(primitiveComponent->GetSceneProxy() == nullptr)
        {
            PrimitiveSceneProxy* proxy = primitiveComponent->CreateSceneProxy();

            m_PrimitiveSceneProxyOwners.emplace_back(proxy);
            m_PrimitiveSceneProxies.push_back(proxy);
        }
        else
        {
            // TODO: This is a big hack!!!
            // TODO: currently, the logic below can work properly for single threaded scenario. If we want to support multi-threaded rendering in the future, we might need to implement a command queue system to avoid direct modification of scene proxies in game thread.
            // Update existing scene proxy
            // Simply update the transform for now. TODO: update other data if needed // P.S. we should not get transform from owner GameObject here. This is just a temporary design.
            PrimitiveSceneProxy* proxy = primitiveComponent->GetSceneProxy();
            proxy->m_Transform = primitiveComponent->GetOwner()->GetTransform();
            proxy->m_CastShadow = primitiveComponent->CastShadow();
            StaticMeshComponent* staticMeshComp = dynamic_cast<StaticMeshComponent*>(primitiveComponent);
            if (staticMeshComp)
            {
                StaticMeshSceneProxy* staticMeshProxy = dynamic_cast<StaticMeshSceneProxy*>(proxy);
                if (staticMeshProxy)
                {
                    staticMeshProxy->m_VertexBuffer = staticMeshComp->GetMesh() ? staticMeshComp->GetMesh()->m_VertexBuffer.get() : nullptr;
                    staticMeshProxy->m_VertexDefinition = staticMeshComp->GetMesh() ? staticMeshComp->GetMesh()->m_VertexDefinition.get() : nullptr;
                    staticMeshProxy->m_IndexBuffer = staticMeshComp->GetMesh() ? staticMeshComp->GetMesh()->m_IndexBuffer.get() : nullptr;
                    staticMeshProxy->m_Material = staticMeshComp->GetMaterial();
                }

            }
        }
    }

    void RenderScene::RemovePrimitive(const PrimitiveComponent* primitiveComponent)
    {
        if (!primitiveComponent)
        {
            return;
        }

        m_PrimitiveSceneProxies.erase(
            std::remove_if(
                m_PrimitiveSceneProxies.begin(),
                m_PrimitiveSceneProxies.end(),
                [primitiveComponent](PrimitiveSceneProxy* proxy)
                {
                    return proxy && proxy->m_PrimitiveComponent == primitiveComponent;
                }),
            m_PrimitiveSceneProxies.end());

        m_PrimitiveSceneProxyOwners.erase(
            std::remove_if(
                m_PrimitiveSceneProxyOwners.begin(),
                m_PrimitiveSceneProxyOwners.end(),
                [primitiveComponent](const std::unique_ptr<PrimitiveSceneProxy>& proxy)
                {
                    return proxy && proxy->m_PrimitiveComponent == primitiveComponent;
                }),
            m_PrimitiveSceneProxyOwners.end());
    }

    void RenderScene::UpdateLight(LightComponent *lightComponent)
    {
        if(lightComponent->GetSceneProxy() == nullptr)
        {
            switch(lightComponent->GetLightType())
            {
            case LightType::Directional:
                {
                    DirectionalLightSceneProxy* proxy = static_cast<DirectionalLightSceneProxy*>(lightComponent->CreateSceneProxy());
                    m_LightSceneProxyOwners.emplace_back(proxy);
                    m_DirectionalLightSceneProxies.push_back(proxy);
                }
                break;

            case LightType::Point:
                {
                    PointLightSceneProxy* proxy = static_cast<PointLightSceneProxy*>(lightComponent->CreateSceneProxy());
                    m_LightSceneProxyOwners.emplace_back(proxy);
                    m_PointLightSceneProxies.push_back(proxy);
                }
                break;

            case LightType::Spot:
                {
                    SpotLightSceneProxy* proxy = static_cast<SpotLightSceneProxy*>(lightComponent->CreateSceneProxy());
                    m_LightSceneProxyOwners.emplace_back(proxy);
                    m_SpotLightSceneProxies.push_back(proxy);
                }
                break;

            default:
                ME_CORE_ERROR("Unknown LightType in RenderScene::UpdateLight");
                break;
            }

        }
        else
        {
            // TODO: currently, the logic below can work properly for single threaded scenario. If we want to support multi-threaded rendering in the future, we might need to implement a command queue system to avoid direct modification of scene proxies in game thread.
            // Update existing scene proxy
            // Keep the scene proxy in sync when light properties are changed.
            LightSceneProxy* sceneProxy = lightComponent->GetSceneProxy();
            sceneProxy->m_Position = lightComponent->GetPosition();
            sceneProxy->m_LightColor = lightComponent->GetLightColor();
            sceneProxy->m_Intensity = lightComponent->GetIntensity();
            sceneProxy->m_DiffuseFactor = lightComponent->GetDiffuseFactor();
            sceneProxy->m_SpecularFactor = lightComponent->GetSpecularFactor();
            sceneProxy->m_CastsShadow = lightComponent->CastShadow();

            if(lightComponent->GetLightType() == LightType::Directional)
            {
                auto* dirComp = static_cast<DirectionalLightComponent*>(lightComponent);
                auto* dirProxy = static_cast<DirectionalLightSceneProxy*>(sceneProxy);
                dirProxy->m_Direction = dirComp->GetDirection();
            }
            else if(lightComponent->GetLightType() == LightType::Spot)
            {
                auto* spotComp = static_cast<SpotLightComponent*>(lightComponent);
                auto* spotProxy = static_cast<SpotLightSceneProxy*>(sceneProxy);
                spotProxy->m_Direction = spotComp->GetDirection();
                spotProxy->m_InnerConeAngle = spotComp->GetInnerConeAngle();
                spotProxy->m_OuterConeAngle = spotComp->GetOuterConeAngle();
            }
        }
    }

    void RenderScene::RemoveLight(const LightComponent* lightComponent)
    {
        if (!lightComponent)
        {
            return;
        }

        m_DirectionalLightSceneProxies.erase(
            std::remove_if(
                m_DirectionalLightSceneProxies.begin(),
                m_DirectionalLightSceneProxies.end(),
                [lightComponent](DirectionalLightSceneProxy* proxy)
                {
                    return proxy && proxy->m_LightComponent == lightComponent;
                }),
            m_DirectionalLightSceneProxies.end());

        m_PointLightSceneProxies.erase(
            std::remove_if(
                m_PointLightSceneProxies.begin(),
                m_PointLightSceneProxies.end(),
                [lightComponent](PointLightSceneProxy* proxy)
                {
                    return proxy && proxy->m_LightComponent == lightComponent;
                }),
            m_PointLightSceneProxies.end());

        m_SpotLightSceneProxies.erase(
            std::remove_if(
                m_SpotLightSceneProxies.begin(),
                m_SpotLightSceneProxies.end(),
                [lightComponent](SpotLightSceneProxy* proxy)
                {
                    return proxy && proxy->m_LightComponent == lightComponent;
                }),
            m_SpotLightSceneProxies.end());

        m_LightSceneProxyOwners.erase(
            std::remove_if(
                m_LightSceneProxyOwners.begin(),
                m_LightSceneProxyOwners.end(),
                [lightComponent](const std::unique_ptr<LightSceneProxy>& proxy)
                {
                    return proxy && proxy->m_LightComponent == lightComponent;
                }),
            m_LightSceneProxyOwners.end());
    }

    void RenderScene::CollectOrphanedSceneProxies()
    {
        m_PrimitiveSceneProxies.erase(
            std::remove_if(
                m_PrimitiveSceneProxies.begin(),
                m_PrimitiveSceneProxies.end(),
                [](PrimitiveSceneProxy* proxy)
                {
                    return (proxy == nullptr) || (proxy->m_PrimitiveComponent == nullptr);
                }),
            m_PrimitiveSceneProxies.end());

        m_DirectionalLightSceneProxies.erase(
            std::remove_if(
                m_DirectionalLightSceneProxies.begin(),
                m_DirectionalLightSceneProxies.end(),
                [](DirectionalLightSceneProxy* proxy)
                {
                    return (proxy == nullptr) || (proxy->m_LightComponent == nullptr);
                }),
            m_DirectionalLightSceneProxies.end());

        m_PointLightSceneProxies.erase(
            std::remove_if(
                m_PointLightSceneProxies.begin(),
                m_PointLightSceneProxies.end(),
                [](PointLightSceneProxy* proxy)
                {
                    return (proxy == nullptr) || (proxy->m_LightComponent == nullptr);
                }),
            m_PointLightSceneProxies.end());

        m_SpotLightSceneProxies.erase(
            std::remove_if(
                m_SpotLightSceneProxies.begin(),
                m_SpotLightSceneProxies.end(),
                [](SpotLightSceneProxy* proxy)
                {
                    return (proxy == nullptr) || (proxy->m_LightComponent == nullptr);
                }),
            m_SpotLightSceneProxies.end());

        m_PrimitiveSceneProxyOwners.erase(
            std::remove_if(
                m_PrimitiveSceneProxyOwners.begin(),
                m_PrimitiveSceneProxyOwners.end(),
                [](const std::unique_ptr<PrimitiveSceneProxy>& proxy)
                {
                    return (!proxy) || (proxy->m_PrimitiveComponent == nullptr);
                }),
            m_PrimitiveSceneProxyOwners.end());

        m_LightSceneProxyOwners.erase(
            std::remove_if(
                m_LightSceneProxyOwners.begin(),
                m_LightSceneProxyOwners.end(),
                [](const std::unique_ptr<LightSceneProxy>& proxy)
                {
                    return (!proxy) || (proxy->m_LightComponent == nullptr);
                }),
            m_LightSceneProxyOwners.end());
    }
}
