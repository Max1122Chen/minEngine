#include "RenderScene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"   // TODO: maybe remove this include later
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/PrimitiveSceneProxy.h"

#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Render/DirectionalLightSceneProxy.h"
#include "Runtime/Function/Render/PointLightSceneProxy.h"
#include "Runtime/Function/Render/SpotLightSceneProxy.h"


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
            // Simply update the position for now. TODO: update other data if needed
            LightSceneProxy* sceneProxy = lightComponent->GetSceneProxy();
            sceneProxy->m_Position = lightComponent->GetPosition();
        }
    }
}
