#include "RenderScene.h"

#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Framework/Components/SkyBoxComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/PointLightComponent.h"
#include "Runtime/Function/Framework/Components/SpotLightComponent.h"
#include "Runtime/Function/Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/SpotLightSceneProxy.h"

#include <algorithm>

namespace minEngine
{
    RenderScene::~RenderScene() = default;

    void RenderScene::ApplyAddPrimitive_RT(PrimitiveSceneProxy* proxy)
    {
        if (proxy == nullptr)
        {
            return;
        }
        m_PrimitiveSceneProxyOwners.emplace_back(proxy);
        m_PrimitiveSceneProxies.push_back(proxy);
    }

    void RenderScene::ApplyUpdatePrimitiveTransform_RT(const PrimitiveTransformUpdateParams& params)
    {
        if (params.Proxy == nullptr)
        {
            return;
        }
        params.Proxy->m_Transform = params.Transform;
        params.Proxy->m_CastShadow = params.CastShadow;
        params.Proxy->m_Enabled = params.Enabled;
    }

    void RenderScene::ApplyRemovePrimitiveProxy_RT(PrimitiveSceneProxy* proxy)
    {
        if (proxy == nullptr)
        {
            return;
        }

        m_PrimitiveSceneProxies.erase(
            std::remove(m_PrimitiveSceneProxies.begin(), m_PrimitiveSceneProxies.end(), proxy),
            m_PrimitiveSceneProxies.end());

        m_PrimitiveSceneProxyOwners.erase(
            std::remove_if(
                m_PrimitiveSceneProxyOwners.begin(),
                m_PrimitiveSceneProxyOwners.end(),
                [proxy](const std::unique_ptr<PrimitiveSceneProxy>& owned)
                {
                    return owned.get() == proxy;
                }),
            m_PrimitiveSceneProxyOwners.end());
    }

    void RenderScene::ApplyRemovePrimitiveComponent_RT(const PrimitiveComponent* primitiveComponent)
    {
        if (primitiveComponent == nullptr)
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

    void RenderScene::AddPrimitive(PrimitiveComponent* primitiveComponent)
    {
        if (primitiveComponent == nullptr || !primitiveComponent->IsActive())
        {
            return;
        }
        if (primitiveComponent->GetSceneProxy() != nullptr)
        {
            ME_LOG(LogRender, Warn, "RenderScene::AddPrimitive: proxy already exists; use RecreatePrimitive.");
            return;
        }

        PrimitiveSceneProxy* proxy = primitiveComponent->CreateSceneProxy();
        if (proxy == nullptr)
        {
            return;
        }
        proxy->m_Enabled = primitiveComponent->IsActiveAndVisible();
        EnqueueRenderCommand([this, proxy]() {
            ApplyAddPrimitive_RT(proxy);
        });
    }

    void RenderScene::UpdatePrimitiveTransform(PrimitiveComponent* primitiveComponent)
    {
        if (primitiveComponent == nullptr || !primitiveComponent->IsActive())
        {
            return;
        }

        PrimitiveSceneProxy* proxy = primitiveComponent->GetSceneProxy();
        if (proxy == nullptr)
        {
            return;
        }

        PrimitiveTransformUpdateParams params{};
        params.Proxy = proxy;
        params.Transform = primitiveComponent->GetWorldTransform();
        params.CastShadow = primitiveComponent->CastShadow();
        params.Enabled = primitiveComponent->IsActiveAndVisible();
        EnqueueRenderCommand([this, params]() {
            ApplyUpdatePrimitiveTransform_RT(params);
        });
    }

    void RenderScene::RecreatePrimitive(PrimitiveComponent* primitiveComponent)
    {
        if (primitiveComponent == nullptr || !primitiveComponent->IsActive())
        {
            return;
        }

        if (PrimitiveSceneProxy* oldProxy = primitiveComponent->GetSceneProxy())
        {
            // Do not clear oldProxy->m_PrimitiveComponent here. Draw may still run before
            // FlushApply, and the main collector skips proxies with a null component link.
            primitiveComponent->DetachSceneProxy();
            EnqueueRenderCommand([this, oldProxy]() {
                oldProxy->m_PrimitiveComponent = nullptr;
                ApplyRemovePrimitiveProxy_RT(oldProxy);
            });
        }

        AddPrimitive(primitiveComponent);
    }

    void RenderScene::RemovePrimitive(const PrimitiveComponent* primitiveComponent)
    {
        if (!primitiveComponent)
        {
            return;
        }

        EnqueueRenderCommand([this, primitiveComponent]() {
            ApplyRemovePrimitiveComponent_RT(primitiveComponent);
        });
    }

    void RenderScene::ApplyAddLight_RT(LightSceneProxy* proxy, LightType lightType)
    {
        if (proxy == nullptr)
        {
            return;
        }
        m_LightSceneProxyOwners.emplace_back(proxy);
        switch (lightType)
        {
        case LightType::Directional:
            m_DirectionalLightSceneProxies.push_back(static_cast<DirectionalLightSceneProxy*>(proxy));
            break;
        case LightType::Point:
            m_PointLightSceneProxies.push_back(static_cast<PointLightSceneProxy*>(proxy));
            break;
        case LightType::Spot:
            m_SpotLightSceneProxies.push_back(static_cast<SpotLightSceneProxy*>(proxy));
            break;
        default:
            break;
        }
    }

    void RenderScene::ApplyUpdateLightTransform_RT(const LightTransformUpdateParams& params)
    {
        if (params.Proxy == nullptr)
        {
            return;
        }

        params.Proxy->m_Position = params.Position;
        params.Proxy->m_Enabled = params.Enabled;

        if (params.Type == LightType::Directional)
        {
            static_cast<DirectionalLightSceneProxy*>(params.Proxy)->m_Direction = params.Direction;
        }
        else if (params.Type == LightType::Spot)
        {
            static_cast<SpotLightSceneProxy*>(params.Proxy)->m_Direction = params.Direction;
        }
    }

    void RenderScene::ApplyUpdateLightColor_RT(const LightColorUpdateParams& params)
    {
        if (params.Proxy == nullptr)
        {
            return;
        }
        params.Proxy->m_LightColor = params.LightColor;
        params.Proxy->m_Intensity = params.Intensity;
        params.Proxy->m_DiffuseFactor = params.DiffuseFactor;
        params.Proxy->m_SpecularFactor = params.SpecularFactor;
    }

    void RenderScene::ApplyRemoveLightProxy_RT(LightSceneProxy* proxy)
    {
        if (proxy == nullptr)
        {
            return;
        }

        m_DirectionalLightSceneProxies.erase(
            std::remove_if(
                m_DirectionalLightSceneProxies.begin(),
                m_DirectionalLightSceneProxies.end(),
                [proxy](DirectionalLightSceneProxy* entry)
                {
                    return static_cast<LightSceneProxy*>(entry) == proxy;
                }),
            m_DirectionalLightSceneProxies.end());
        m_PointLightSceneProxies.erase(
            std::remove_if(
                m_PointLightSceneProxies.begin(),
                m_PointLightSceneProxies.end(),
                [proxy](PointLightSceneProxy* entry)
                {
                    return static_cast<LightSceneProxy*>(entry) == proxy;
                }),
            m_PointLightSceneProxies.end());
        m_SpotLightSceneProxies.erase(
            std::remove_if(
                m_SpotLightSceneProxies.begin(),
                m_SpotLightSceneProxies.end(),
                [proxy](SpotLightSceneProxy* entry)
                {
                    return static_cast<LightSceneProxy*>(entry) == proxy;
                }),
            m_SpotLightSceneProxies.end());

        m_LightSceneProxyOwners.erase(
            std::remove_if(
                m_LightSceneProxyOwners.begin(),
                m_LightSceneProxyOwners.end(),
                [proxy](const std::unique_ptr<LightSceneProxy>& owned)
                {
                    return owned.get() == proxy;
                }),
            m_LightSceneProxyOwners.end());
    }

    void RenderScene::ApplyRemoveLightComponent_RT(const LightComponent* lightComponent)
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

    void RenderScene::AddLight(LightComponent* lightComponent)
    {
        if (lightComponent == nullptr || !lightComponent->IsActive())
        {
            return;
        }
        if (lightComponent->GetSceneProxy() != nullptr)
        {
            ME_LOG(LogRender, Warn, "RenderScene::AddLight: proxy already exists; use RecreateLight.");
            return;
        }

        const LightType lightType = lightComponent->GetLightType();
        LightSceneProxy* proxy = lightComponent->CreateSceneProxy();
        if (proxy == nullptr)
        {
            ME_LOG(LogRender, Error, "Unknown LightType in RenderScene::AddLight");
            return;
        }
        proxy->m_Enabled = lightComponent->IsActiveAndVisible();
        EnqueueRenderCommand([this, proxy, lightType]() {
            ApplyAddLight_RT(proxy, lightType);
        });
    }

    void RenderScene::UpdateLightTransform(LightComponent* lightComponent)
    {
        if (lightComponent == nullptr || !lightComponent->IsActive())
        {
            return;
        }

        LightSceneProxy* proxy = lightComponent->GetSceneProxy();
        if (proxy == nullptr)
        {
            return;
        }

        LightTransformUpdateParams params{};
        params.Proxy = proxy;
        params.Type = lightComponent->GetLightType();
        params.Position = lightComponent->GetWorldPosition();
        params.Enabled = lightComponent->IsActiveAndVisible();

        if (params.Type == LightType::Directional)
        {
            params.Direction = static_cast<DirectionalLightComponent*>(lightComponent)->GetDirection();
        }
        else if (params.Type == LightType::Spot)
        {
            params.Direction = static_cast<SpotLightComponent*>(lightComponent)->GetDirection();
        }

        EnqueueRenderCommand([this, params]() {
            ApplyUpdateLightTransform_RT(params);
        });
    }

    void RenderScene::UpdateLightColor(LightComponent* lightComponent)
    {
        if (lightComponent == nullptr || !lightComponent->IsActive())
        {
            return;
        }

        LightSceneProxy* proxy = lightComponent->GetSceneProxy();
        if (proxy == nullptr)
        {
            return;
        }

        LightColorUpdateParams params{};
        params.Proxy = proxy;
        params.LightColor = lightComponent->GetLightColor().ToVector3();
        params.Intensity = lightComponent->GetIntensity();
        params.DiffuseFactor = lightComponent->GetDiffuseFactor();
        params.SpecularFactor = lightComponent->GetSpecularFactor();
        EnqueueRenderCommand([this, params]() {
            ApplyUpdateLightColor_RT(params);
        });
    }

    void RenderScene::RecreateLight(LightComponent* lightComponent)
    {
        if (lightComponent == nullptr || !lightComponent->IsActive())
        {
            return;
        }

        if (LightSceneProxy* oldProxy = lightComponent->GetSceneProxy())
        {
            oldProxy->m_LightComponent = nullptr;
            lightComponent->DetachSceneProxy();
            EnqueueRenderCommand([this, oldProxy]() {
                ApplyRemoveLightProxy_RT(oldProxy);
            });
        }

        AddLight(lightComponent);
    }

    void RenderScene::RemoveLight(const LightComponent* lightComponent)
    {
        if (!lightComponent)
        {
            return;
        }

        EnqueueRenderCommand([this, lightComponent]() {
            ApplyRemoveLightComponent_RT(lightComponent);
        });
    }

    void RenderScene::ApplySetSkyBox_RT(SkyBoxSceneProxy* proxy)
    {
        if (proxy == nullptr)
        {
            return;
        }
        if (m_SkyBoxProxy && m_SkyBoxProxy != proxy)
        {
            ME_LOG(LogRender, Warn, "RenderScene: replacing existing SkyBox (only one allowed per scene).");
            m_SkyBoxProxy = nullptr;
            m_SkyBoxProxyOwner.reset();
        }
        m_SkyBoxProxyOwner.reset(proxy);
        m_SkyBoxProxy = proxy;
    }

    void RenderScene::ApplyRemoveSkyBox_RT(SkyBoxComponent* skyBoxComponent)
    {
        if (!skyBoxComponent || !m_SkyBoxProxy)
        {
            return;
        }
        if (m_SkyBoxProxy->m_SkyBoxComponent != skyBoxComponent)
        {
            return;
        }
        m_SkyBoxProxy->m_SkyBoxComponent = nullptr;
        m_SkyBoxProxy = nullptr;
        m_SkyBoxProxyOwner.reset();
    }

    void RenderScene::SetSkyBox(SkyBoxComponent* skyBoxComponent)
    {
        if (skyBoxComponent == nullptr || !skyBoxComponent->IsActive())
        {
            return;
        }
        if (skyBoxComponent->GetSceneProxy() != nullptr)
        {
            ME_LOG(LogRender, Warn, "RenderScene::SetSkyBox: proxy already exists; use RecreateSkyBox.");
            return;
        }

        if (m_SkyBoxProxy && m_SkyBoxProxy->m_SkyBoxComponent != skyBoxComponent)
        {
            RemoveSkyBox(m_SkyBoxProxy->m_SkyBoxComponent);
        }

        SkyBoxSceneProxy* proxy = skyBoxComponent->CreateSceneProxy();
        if (proxy == nullptr)
        {
            return;
        }
        EnqueueRenderCommand([this, proxy]() {
            ApplySetSkyBox_RT(proxy);
        });
    }

    void RenderScene::RecreateSkyBox(SkyBoxComponent* skyBoxComponent)
    {
        if (skyBoxComponent == nullptr || !skyBoxComponent->IsActive())
        {
            return;
        }

        if (skyBoxComponent->GetSceneProxy() != nullptr)
        {
            RemoveSkyBox(skyBoxComponent);
        }

        SetSkyBox(skyBoxComponent);
    }

    void RenderScene::RemoveSkyBox(SkyBoxComponent* skyBoxComponent)
    {
        if (!skyBoxComponent)
        {
            return;
        }

        skyBoxComponent->DetachSceneProxy();
        EnqueueRenderCommand([this, skyBoxComponent]() {
            ApplyRemoveSkyBox_RT(skyBoxComponent);
        });
    }

    void RenderScene::ApplyAddWidget_RT(WidgetSceneProxy* proxy)
    {
        if (proxy == nullptr)
        {
            return;
        }
        m_WidgetSceneProxyOwners.emplace_back(proxy);
        m_WidgetSceneProxies.push_back(proxy);
    }

    void RenderScene::ApplyRemoveWidgetProxy_RT(WidgetSceneProxy* proxy)
    {
        if (proxy == nullptr)
        {
            return;
        }

        m_WidgetSceneProxies.erase(
            std::remove(m_WidgetSceneProxies.begin(), m_WidgetSceneProxies.end(), proxy),
            m_WidgetSceneProxies.end());

        m_WidgetSceneProxyOwners.erase(
            std::remove_if(
                m_WidgetSceneProxyOwners.begin(),
                m_WidgetSceneProxyOwners.end(),
                [proxy](const std::unique_ptr<WidgetSceneProxy>& owned)
                {
                    return owned.get() == proxy;
                }),
            m_WidgetSceneProxyOwners.end());
    }

    void RenderScene::ApplyRemoveWidgetComponent_RT(WidgetComponent* widgetComponent)
    {
        if (!widgetComponent)
        {
            return;
        }

        m_WidgetSceneProxies.erase(
            std::remove_if(
                m_WidgetSceneProxies.begin(),
                m_WidgetSceneProxies.end(),
                [widgetComponent](WidgetSceneProxy* proxy)
                {
                    return proxy && proxy->m_WidgetComponent == widgetComponent;
                }),
            m_WidgetSceneProxies.end());

        m_WidgetSceneProxyOwners.erase(
            std::remove_if(
                m_WidgetSceneProxyOwners.begin(),
                m_WidgetSceneProxyOwners.end(),
                [widgetComponent](const std::unique_ptr<WidgetSceneProxy>& proxy)
                {
                    return proxy && proxy->m_WidgetComponent == widgetComponent;
                }),
            m_WidgetSceneProxyOwners.end());
    }

    void RenderScene::AddWidget(WidgetComponent* widgetComponent)
    {
        if (widgetComponent == nullptr || !widgetComponent->IsActive())
        {
            return;
        }
        if (widgetComponent->GetSceneProxy() != nullptr)
        {
            ME_LOG(LogRender, Warn, "RenderScene::AddWidget: proxy already exists; use RecreateWidget.");
            return;
        }

        WidgetSceneProxy* proxy = widgetComponent->CreateSceneProxy();
        if (proxy == nullptr)
        {
            return;
        }
        EnqueueRenderCommand([this, proxy]() {
            ApplyAddWidget_RT(proxy);
        });
    }

    void RenderScene::RecreateWidget(WidgetComponent* widgetComponent)
    {
        if (widgetComponent == nullptr || !widgetComponent->IsActive())
        {
            return;
        }

        if (WidgetSceneProxy* oldProxy = widgetComponent->GetSceneProxy())
        {
            oldProxy->m_WidgetComponent = nullptr;
            widgetComponent->DetachSceneProxy();
            EnqueueRenderCommand([this, oldProxy]() {
                ApplyRemoveWidgetProxy_RT(oldProxy);
            });
        }

        AddWidget(widgetComponent);
    }

    void RenderScene::RemoveWidget(WidgetComponent* widgetComponent)
    {
        if (!widgetComponent)
        {
            return;
        }

        widgetComponent->DetachSceneProxy();
        EnqueueRenderCommand([this, widgetComponent]() {
            ApplyRemoveWidgetComponent_RT(widgetComponent);
        });
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

        m_WidgetSceneProxies.erase(
            std::remove_if(
                m_WidgetSceneProxies.begin(),
                m_WidgetSceneProxies.end(),
                [](WidgetSceneProxy* proxy)
                {
                    return (proxy == nullptr) || (proxy->m_WidgetComponent == nullptr);
                }),
            m_WidgetSceneProxies.end());

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

        m_WidgetSceneProxyOwners.erase(
            std::remove_if(
                m_WidgetSceneProxyOwners.begin(),
                m_WidgetSceneProxyOwners.end(),
                [](const std::unique_ptr<WidgetSceneProxy>& proxy)
                {
                    return (!proxy) || (proxy->m_WidgetComponent == nullptr);
                }),
            m_WidgetSceneProxyOwners.end());

        if (m_SkyBoxProxy && m_SkyBoxProxy->m_SkyBoxComponent == nullptr)
        {
            m_SkyBoxProxy = nullptr;
            m_SkyBoxProxyOwner.reset();
        }
    }
}
