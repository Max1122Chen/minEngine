#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"
#include "Runtime/Function/Render/SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "Runtime/Function/Render/SceneProxies/WidgetSceneProxy.h"
#include "Runtime/Core/Math/Math.h"

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
    enum class LightType : uint8_t;

    /**
     * Game-thread registration surface for render-side scene proxies.
     * Explicit Add / UpdateTransform / UpdateColor / Recreate / Remove — no family-wide snapshots.
     */
    class MINENGINE_API RenderScene
    {
    public:
        RenderScene() = default;
        virtual ~RenderScene();

        void AddPrimitive(PrimitiveComponent* primitiveComponent);
        void UpdatePrimitiveTransform(PrimitiveComponent* primitiveComponent);
        void RecreatePrimitive(PrimitiveComponent* primitiveComponent);
        void RemovePrimitive(const PrimitiveComponent* primitiveComponent);

        void AddLight(LightComponent* lightComponent);
        void UpdateLightTransform(LightComponent* lightComponent);
        void UpdateLightColor(LightComponent* lightComponent);
        void RecreateLight(LightComponent* lightComponent);
        void RemoveLight(const LightComponent* lightComponent);

        void SetSkyBox(SkyBoxComponent* skyBoxComponent);
        void RecreateSkyBox(SkyBoxComponent* skyBoxComponent);
        void RemoveSkyBox(SkyBoxComponent* skyBoxComponent);
        SkyBoxSceneProxy* GetSkyBoxProxy() const { return m_SkyBoxProxy; }

        void AddWidget(WidgetComponent* widgetComponent);
        void RecreateWidget(WidgetComponent* widgetComponent);
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
        /** Transform fast path — UE FPrimitiveUpdateParams analogue (MVP subset). */
        struct PrimitiveTransformUpdateParams
        {
            PrimitiveSceneProxy* Proxy = nullptr;
            Transform Transform{};
            bool CastShadow = true;
            bool Enabled = true;
        };

        /** UE FUpdateLightTransformParameters analogue (MVP subset). */
        struct LightTransformUpdateParams
        {
            LightSceneProxy* Proxy = nullptr;
            LightType Type{};
            Vector3 Position{};
            Vector3 Direction{};
            bool Enabled = true;
        };

        /** UE FUpdateLightColorParameters analogue (fields clipped to current proxies). */
        struct LightColorUpdateParams
        {
            LightSceneProxy* Proxy = nullptr;
            Vector3 LightColor{};
            float Intensity = 0.0f;
            float DiffuseFactor = 0.0f;
            float SpecularFactor = 0.0f;
        };

        void ApplyAddPrimitive_RT(PrimitiveSceneProxy* proxy);
        void ApplyUpdatePrimitiveTransform_RT(const PrimitiveTransformUpdateParams& params);
        void ApplyRemovePrimitiveProxy_RT(PrimitiveSceneProxy* proxy);
        void ApplyRemovePrimitiveComponent_RT(const PrimitiveComponent* primitiveComponent);

        void ApplyAddLight_RT(LightSceneProxy* proxy, LightType lightType);
        void ApplyUpdateLightTransform_RT(const LightTransformUpdateParams& params);
        void ApplyUpdateLightColor_RT(const LightColorUpdateParams& params);
        void ApplyRemoveLightProxy_RT(LightSceneProxy* proxy);
        void ApplyRemoveLightComponent_RT(const LightComponent* lightComponent);

        void ApplySetSkyBox_RT(SkyBoxSceneProxy* proxy);
        void ApplyRemoveSkyBox_RT(SkyBoxComponent* skyBoxComponent);

        void ApplyAddWidget_RT(WidgetSceneProxy* proxy);
        void ApplyRemoveWidgetProxy_RT(WidgetSceneProxy* proxy);
        void ApplyRemoveWidgetComponent_RT(WidgetComponent* widgetComponent);

        std::vector<std::unique_ptr<PrimitiveSceneProxy>> m_PrimitiveSceneProxyOwners;
        std::vector<std::unique_ptr<LightSceneProxy>> m_LightSceneProxyOwners;
        std::vector<std::unique_ptr<WidgetSceneProxy>> m_WidgetSceneProxyOwners;
        std::unique_ptr<SkyBoxSceneProxy> m_SkyBoxProxyOwner;
    };
}
