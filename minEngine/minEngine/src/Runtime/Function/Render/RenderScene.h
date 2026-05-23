#pragma once
#include "Core.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"
#include "Runtime/Function/Render/SkyBoxSceneProxies/SkyBoxSceneProxy.h"

namespace minEngine
{
    class PrimitiveComponent;
    class LightComponent;
    class DirectionalLightSceneProxy;
    class PointLightSceneProxy;
    class SpotLightSceneProxy;
    class SkyBoxComponent;

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

        void CollectOrphanedSceneProxies();

        // Light scene proxies
        std::vector<DirectionalLightSceneProxy*> m_DirectionalLightSceneProxies;
        std::vector<PointLightSceneProxy*> m_PointLightSceneProxies;
        std::vector<SpotLightSceneProxy*> m_SpotLightSceneProxies;

        // Primitive scene proxies
        std::vector<PrimitiveSceneProxy*> m_PrimitiveSceneProxies;

        SkyBoxSceneProxy* m_SkyBoxProxy = nullptr;

    private:
        // Single ownership of all scene proxies; raw-pointer vectors above are non-owning views.
        std::vector<std::unique_ptr<PrimitiveSceneProxy>> m_PrimitiveSceneProxyOwners;
        std::vector<std::unique_ptr<LightSceneProxy>> m_LightSceneProxyOwners;
        std::unique_ptr<SkyBoxSceneProxy> m_SkyBoxProxyOwner;
    };
}