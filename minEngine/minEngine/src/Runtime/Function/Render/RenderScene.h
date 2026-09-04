#pragma once
#include "Core.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"
#include "Runtime/Function/Render/SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "Runtime/Function/Render/SceneProxies/WidgetSceneProxy.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class PrimitiveComponent;
    class LightComponent;
    class DirectionalLightSceneProxy;
    class PointLightSceneProxy;
    class SpotLightSceneProxy;
    class SkyBoxComponent;
    class WidgetComponent;

    class RenderScene
    {
    public:
        RenderScene() = default;
        virtual ~RenderScene();

        void UpdatePrimitive(PrimitiveComponent* primitiveComponent);
        void RemovePrimitive(const PrimitiveComponent* primitiveComponent);

        void UpdateLight(LightComponent* lightComponent);
        void RemoveLight(const LightComponent* lightComponent);

        void UpdateSkyBox(SkyBoxComponent* skyBoxComponent);
        void RemoveSkyBox(SkyBoxComponent* skyBoxComponent);
        SkyBoxSceneProxy* GetSkyBoxProxy() const { return m_SkyBoxProxy; }

        void UpdateWidget(WidgetComponent* widgetComponent);
        void RemoveWidget(WidgetComponent* widgetComponent);

        void CollectOrphanedSceneProxies();

        std::vector<DirectionalLightSceneProxy*> m_DirectionalLightSceneProxies;
        std::vector<PointLightSceneProxy*> m_PointLightSceneProxies;
        std::vector<SpotLightSceneProxy*> m_SpotLightSceneProxies;

        std::vector<PrimitiveSceneProxy*> m_PrimitiveSceneProxies;

        /** ScreenUI widgets — never enter Opaque/Translucent queues. */
        std::vector<WidgetSceneProxy*> m_WidgetSceneProxies;

        SkyBoxSceneProxy* m_SkyBoxProxy = nullptr;

    private:
        std::vector<std::unique_ptr<PrimitiveSceneProxy>> m_PrimitiveSceneProxyOwners;
        std::vector<std::unique_ptr<LightSceneProxy>> m_LightSceneProxyOwners;
        std::vector<std::unique_ptr<WidgetSceneProxy>> m_WidgetSceneProxyOwners;
        std::unique_ptr<SkyBoxSceneProxy> m_SkyBoxProxyOwner;
    };
}
