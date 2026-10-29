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

        /**
         * End of the render frame: destroy the proxies that removal commands parked during it.
         *
         * Render role only. Runs after the frame was recorded, which is the first moment no draw
         * command of this frame can still be built from a proxy this frees, and no later drain can
         * name one either: the game role tags new commands for a later frame.
         */
        static void FlushPendingProxyDeletes();

        /**
         * True while this scene still owns a proxy or parks one for the frame-end delete.
         *
         * Only a removal empties a scene: it takes the proxy off the draw lists and parks it, the
         * frame end destroys what was parked. A scene that reports true therefore has removal
         * commands in flight and may not be destroyed yet - its proxies would be released with it
         * while the drain that was supposed to apply those removals has not run.
         */
        bool HasPendingProxyWork() const
        {
            return !m_PrimitiveSceneProxyOwners.empty() || !m_LightSceneProxyOwners.empty() ||
                !m_WidgetSceneProxyOwners.empty() || m_SkyBoxProxyOwner != nullptr ||
                !m_PendingDeletePrimitiveSceneProxies.empty() ||
                !m_PendingDeleteWidgetSceneProxies.empty() ||
                !m_PendingDeleteLightSceneProxies.empty() || m_PendingDeleteSkyBoxSceneProxy != nullptr;
        }

        /** Proxies destroyed by the frame-end delete so far. Diagnostics and tests. */
        uint64_t m_ProxyDeleteCount = 0;

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
            /** World box computed by the game role; the render role never derives it itself. */
            Math::Geometry::AABB WorldBounds{};
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

        /**
         * Proxies a removal command already took off the draw lists, still owned, still alive.
         *
         * A draw command of the frame that recorded carries bare pointers into the proxy that
         * produced it, so a removal may not destroy one: it stops registering the proxy and parks
         * it here, and the frame end destroys it. That is the only place a scene proxy dies.
         */
        std::vector<std::unique_ptr<PrimitiveSceneProxy>> m_PendingDeletePrimitiveSceneProxies;
        std::vector<std::unique_ptr<WidgetSceneProxy>> m_PendingDeleteWidgetSceneProxies;
        std::vector<std::unique_ptr<LightSceneProxy>> m_PendingDeleteLightSceneProxies;
        std::unique_ptr<SkyBoxSceneProxy> m_PendingDeleteSkyBoxSceneProxy;

        /** Wall clock of the oldest parked proxy, or 0 when nothing is parked. */
        uint64_t m_PendingDeleteOldestMs = 0;

        /** True while this scene sits in the frame-end list. */
        bool m_bRegisteredForFrameEndDelete = false;

        std::unique_ptr<SkyBoxSceneProxy> m_SkyBoxProxyOwner;

        friend void DiscardRenderSceneProxiesForTeardown(RenderScene& scene);
    };

    /**
     * Hand a render scene whose owning Scene disappeared to the frame-end delete instead of freeing
     * it where the last reference to that Scene was dropped.
     *
     * A Scene can go away inside a frame - a scratch scene released by its creator, a world torn
     * down by whoever owned it - and the components it destroyed on the way out queued their
     * removals as commands of that frame. Freeing the render scene here would destroy proxies a
     * command still names and a frame may still be recording from, so it keeps its reference until
     * the frame end, which is the only point a scene proxy may die.
     */
    void HandOverOrphanedRenderScene(std::shared_ptr<RenderScene> renderScene);

    /**
     * Free everything the scene still owns, draw lists included, without waiting for a removal
     * command or a frame end.
     *
     * Shutdown only: the caller guarantees no frame is in flight, no drain will apply a command
     * from here on, and the device is still alive. That is what makes collapsing the two steps of
     * a removal - off the draw lists, then destroyed - safe instead of a skipped step.
     */
    void DiscardRenderSceneProxiesForTeardown(RenderScene& scene);

    /** Shutdown: the same for every render scene whose owning Scene is already gone. */
    void DiscardOrphanedRenderScenesForTeardown();
}
