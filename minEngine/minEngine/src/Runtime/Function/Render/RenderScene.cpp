#include "RenderScene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"   // TODO: maybe remove this include later
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"

#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/SpotLightComponent.h"
#include "Runtime/Function/Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/SpotLightSceneProxy.h"


namespace minEngine
{
    void RenderScene::UpdatePrimitive(PrimitiveComponent *primitiveComponent)
    {
        if(primitiveComponent->GetSceneProxy() == nullptr)
        {
            PrimitiveSceneProxy* proxy = primitiveComponent->CreateSceneProxy();
            
            m_PrimitiveSceneProxies.push_back(proxy);
        }
        else
        {
            // Update existing scene proxy

            // Simply update the transform for now. TODO: update other data if needed // P.S. we should not get transform from owner GameObject here. This is just a temporary design.
            primitiveComponent->GetSceneProxy()->m_Transform = primitiveComponent->GetOwner()->GetTransform();
            primitiveComponent->GetSceneProxy()->m_CastShadow = primitiveComponent->CastShadow();
        }
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
                    m_DirectionalLightSceneProxies.push_back(proxy);
                }
                break;

            case LightType::Point:
                {
                    PointLightSceneProxy* proxy = static_cast<PointLightSceneProxy*>(lightComponent->CreateSceneProxy());
                    
                    m_PointLightSceneProxies.push_back(proxy);
                }
                break;

            case LightType::Spot:
                {
                    SpotLightSceneProxy* proxy = static_cast<SpotLightSceneProxy*>(lightComponent->CreateSceneProxy());
                    
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
}
