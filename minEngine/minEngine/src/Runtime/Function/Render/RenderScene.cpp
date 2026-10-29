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
#include <atomic>
#include <chrono>
#include <iterator>
#include <memory>
#include <mutex>

namespace minEngine
{
    namespace
    {
        /** Monotonic milliseconds; the parked-proxy watchdog is measured with it. */
        uint64_t NowMilliseconds()
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }

        /**
         * How long a proxy may stay parked before the frame-end delete is reported as missing.
         * One frame is microseconds; this only fires when frames stop reaching their end.
         */
        constexpr uint64_t kPendingDeleteStallWarnMs = 2000;

        /**
         * The scenes waiting for the frame-end delete.
         *
         * The engine reaches the frame end on the render role, where the only scenes it could
         * enumerate are the ones it is about to draw - and a frame that draws nothing is exactly
         * the case a parked proxy must not be stuck in. So a scene registers itself the moment a
         * removal parks its first proxy, and the list is emptied inside the same frame end.
         *
         * A scene leaves the list in its own destructor: a scene destroyed while it still has
         * parked proxies is a contract violation, and one that must not leave a dangling entry
         * behind for the next frame end to walk into.
         */
        struct FrameEndDeleteRegistry
        {
            std::mutex Mutex;
            std::vector<RenderScene*> Scenes;
            /**
             * Scenes whose owning Scene is already gone, kept alive until an apply has emptied them.
             *
             * A removal command names both the proxy it parks and the scene it writes into, so the
             * scene may not be freed before the command that is still queued for it has run. The
             * frame end is where it goes once nothing is left to apply.
             */
            std::vector<std::shared_ptr<RenderScene>> OrphanedScenes;
            std::atomic<uint64_t> Refusals{ 0 };

            static FrameEndDeleteRegistry& Get()
            {
                static FrameEndDeleteRegistry registry;
                return registry;
            }
        };

        /**
         * Puts a scene in the frame-end list on the proxy that starts its wait clock.
         *
         * Both facts belong to the scene and outlive this call, so they are passed by reference
         * rather than reached through the scene: the parking sites are the only writers.
         */
        void NoteParkedForFrameEnd(RenderScene* scene, uint64_t& oldestMs, bool& registered)
        {
            if (oldestMs == 0)
            {
                oldestMs = NowMilliseconds();
            }
            if (registered)
            {
                return;
            }
            registered = true;

            FrameEndDeleteRegistry& registry = FrameEndDeleteRegistry::Get();
            std::lock_guard<std::mutex> lock(registry.Mutex);
            registry.Scenes.push_back(scene);
        }

        /**
         * The ownership half of a removal: owners matching shouldRemove stop being owned here, and
         * are owned by the parked list instead. Nothing is destroyed.
         */
        template <typename TProxy, typename TShouldRemove>
        void ParkOwnersForFrameEndDelete(
            RenderScene* scene,
            std::vector<std::unique_ptr<TProxy>>& owners,
            TShouldRemove shouldRemove,
            std::vector<std::unique_ptr<TProxy>>& parked,
            uint64_t& oldestMs,
            bool& registered)
        {
            bool bMovedAny = false;
            for (std::unique_ptr<TProxy>& owner : owners)
            {
                if (owner && shouldRemove(owner.get()))
                {
                    parked.push_back(std::move(owner));
                    bMovedAny = true;
                }
            }
            owners.erase(std::remove(owners.begin(), owners.end(), nullptr), owners.end());

            if (bMovedAny)
            {
                NoteParkedForFrameEnd(scene, oldestMs, registered);
            }
        }

        void ParkSkyBoxForFrameEndDelete(
            RenderScene* scene,
            std::unique_ptr<SkyBoxSceneProxy>& owner,
            std::unique_ptr<SkyBoxSceneProxy>& parked,
            uint64_t& oldestMs,
            bool& registered)
        {
            if (!owner)
            {
                return;
            }
            parked = std::move(owner);
            NoteParkedForFrameEnd(scene, oldestMs, registered);
        }
    }

    RenderScene::~RenderScene()
    {
        FrameEndDeleteRegistry& registry = FrameEndDeleteRegistry::Get();
        {
            std::lock_guard<std::mutex> lock(registry.Mutex);
            registry.Scenes.erase(
                std::remove(registry.Scenes.begin(), registry.Scenes.end(), this),
                registry.Scenes.end());
        }

        // A scene going away with proxies still registered is a contract violation, not a case to
        // survive: the owner of a primitive is expected to remove it first, and the world teardown
        // that skipped it has already lost the pointer it would need to do so. Nothing is deleted
        // in bulk here - the owners die with this scene and the count says how many were missed.
        const size_t registeredPrimitives =
            m_PrimitiveSceneProxyOwners.size() + m_PendingDeletePrimitiveSceneProxies.size();
        const size_t registeredWidgets =
            m_WidgetSceneProxyOwners.size() + m_PendingDeleteWidgetSceneProxies.size();
        const size_t registeredLights =
            m_LightSceneProxyOwners.size() + m_PendingDeleteLightSceneProxies.size();
        const size_t registeredSkyBoxes =
            (m_SkyBoxProxyOwner ? 1u : 0u) + (m_PendingDeleteSkyBoxSceneProxy ? 1u : 0u);
        if (registeredPrimitives > 0 || registeredWidgets > 0 || registeredLights > 0 ||
            registeredSkyBoxes > 0)
        {
            ME_LOG(
                LogRender,
                Error,
                "RenderScene destroyed with {} primitive, {} widget, {} light and {} skybox "
                "proxy(ies) still registered or still parked for a frame-end delete. Every "
                "primitive has to be removed before its scene goes away: a removal is what takes a "
                "proxy off the frame's draw lists, and only the end of a frame may destroy it. The "
                "remaining owners are released with this scene.",
                registeredPrimitives,
                registeredWidgets,
                registeredLights,
                registeredSkyBoxes);
        }
    }

    void HandOverOrphanedRenderScene(std::shared_ptr<RenderScene> renderScene)
    {
        if (!renderScene)
        {
            return;
        }

        FrameEndDeleteRegistry& registry = FrameEndDeleteRegistry::Get();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        registry.OrphanedScenes.push_back(std::move(renderScene));
    }

    void DiscardRenderSceneProxiesForTeardown(RenderScene& scene)
    {
        // The same two facts a removal establishes - marked, off the draw lists - and then the
        // delete, in one call. Safe only because the caller guarantees no frame is in flight and no
        // command of an earlier frame is still waiting to be applied.
        size_t discarded = 0;

        for (const std::unique_ptr<PrimitiveSceneProxy>& owner : scene.m_PrimitiveSceneProxyOwners)
        {
            if (owner)
            {
                owner->m_bPendingRemove = true;
                discarded += 1;
            }
        }
        scene.m_PrimitiveSceneProxies.clear();
        discarded += scene.m_PendingDeletePrimitiveSceneProxies.size();
        scene.m_PendingDeletePrimitiveSceneProxies.clear();
        scene.m_PrimitiveSceneProxyOwners.clear();

        scene.m_DirectionalLightSceneProxies.clear();
        scene.m_PointLightSceneProxies.clear();
        scene.m_SpotLightSceneProxies.clear();
        discarded += scene.m_LightSceneProxyOwners.size();
        scene.m_LightSceneProxyOwners.clear();
        discarded += scene.m_PendingDeleteLightSceneProxies.size();
        scene.m_PendingDeleteLightSceneProxies.clear();

        scene.m_WidgetSceneProxies.clear();
        discarded += scene.m_WidgetSceneProxyOwners.size();
        scene.m_WidgetSceneProxyOwners.clear();
        discarded += scene.m_PendingDeleteWidgetSceneProxies.size();
        scene.m_PendingDeleteWidgetSceneProxies.clear();

        discarded += (scene.m_SkyBoxProxyOwner ? 1u : 0u);
        discarded += (scene.m_PendingDeleteSkyBoxSceneProxy ? 1u : 0u);
        scene.m_SkyBoxProxy = nullptr;
        scene.m_SkyBoxProxyOwner.reset();
        scene.m_PendingDeleteSkyBoxSceneProxy.reset();

        scene.m_PendingDeleteOldestMs = 0;
        scene.m_ProxyDeleteCount += discarded;
    }

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
        params.Proxy->m_WorldBounds = params.WorldBounds;
        params.Proxy->m_CastShadow = params.CastShadow;
        params.Proxy->m_Enabled = params.Enabled;
    }

    void RenderScene::ApplyRemovePrimitiveProxy_RT(PrimitiveSceneProxy* proxy)
    {
        if (proxy == nullptr)
        {
            return;
        }

        proxy->m_bPendingRemove = true;
        m_PrimitiveSceneProxies.erase(
            std::remove(m_PrimitiveSceneProxies.begin(), m_PrimitiveSceneProxies.end(), proxy),
            m_PrimitiveSceneProxies.end());

        ParkOwnersForFrameEndDelete(
            this,
            m_PrimitiveSceneProxyOwners,
            [proxy](const PrimitiveSceneProxy* owned)
            {
                return owned == proxy;
            },
            m_PendingDeletePrimitiveSceneProxies,
            m_PendingDeleteOldestMs,
            m_bRegisteredForFrameEndDelete);
    }

    void RenderScene::ApplyRemovePrimitiveComponent_RT(const PrimitiveComponent* primitiveComponent)
    {
        if (primitiveComponent == nullptr)
        {
            return;
        }

        for (PrimitiveSceneProxy* proxy : m_PrimitiveSceneProxies)
        {
            if (proxy != nullptr && proxy->m_PrimitiveComponent == primitiveComponent)
            {
                proxy->m_bPendingRemove = true;
            }
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

        ParkOwnersForFrameEndDelete(
            this,
            m_PrimitiveSceneProxyOwners,
            [primitiveComponent](const PrimitiveSceneProxy* owned)
            {
                return owned->m_PrimitiveComponent == primitiveComponent;
            },
            m_PendingDeletePrimitiveSceneProxies,
            m_PendingDeleteOldestMs,
            m_bRegisteredForFrameEndDelete);
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
        // The box travels with the packet: the game role is the only side that may read the
        // component, and by the time the render role applies this the component may be gone.
        params.WorldBounds =
            Math::Geometry::Transform(primitiveComponent->GetBoundingBox(), primitiveComponent->GetWorldMatrix());
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
            // The old proxy keeps its identity token until this command runs: the draw that may
            // still happen before FlushApply skips it through the removal flag, never by reading
            // game-role state.
            primitiveComponent->DetachSceneProxy();
            EnqueueRenderCommand([this, oldProxy]() {
                oldProxy->m_bPendingRemove = true;
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

        ParkOwnersForFrameEndDelete(
            this,
            m_LightSceneProxyOwners,
            [proxy](const LightSceneProxy* owned)
            {
                return owned == proxy;
            },
            m_PendingDeleteLightSceneProxies,
            m_PendingDeleteOldestMs,
            m_bRegisteredForFrameEndDelete);
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

        ParkOwnersForFrameEndDelete(
            this,
            m_LightSceneProxyOwners,
            [lightComponent](const LightSceneProxy* owned)
            {
                return owned->m_LightComponent == lightComponent;
            },
            m_PendingDeleteLightSceneProxies,
            m_PendingDeleteOldestMs,
            m_bRegisteredForFrameEndDelete);
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
            ParkSkyBoxForFrameEndDelete(
                this,
                m_SkyBoxProxyOwner,
                m_PendingDeleteSkyBoxSceneProxy,
                m_PendingDeleteOldestMs,
                m_bRegisteredForFrameEndDelete);
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
        ParkSkyBoxForFrameEndDelete(
            this,
            m_SkyBoxProxyOwner,
            m_PendingDeleteSkyBoxSceneProxy,
            m_PendingDeleteOldestMs,
            m_bRegisteredForFrameEndDelete);
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

        ParkOwnersForFrameEndDelete(
            this,
            m_WidgetSceneProxyOwners,
            [proxy](const WidgetSceneProxy* owned)
            {
                return owned == proxy;
            },
            m_PendingDeleteWidgetSceneProxies,
            m_PendingDeleteOldestMs,
            m_bRegisteredForFrameEndDelete);
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

        ParkOwnersForFrameEndDelete(
            this,
            m_WidgetSceneProxyOwners,
            [widgetComponent](const WidgetSceneProxy* owned)
            {
                return owned->m_WidgetComponent == widgetComponent;
            },
            m_PendingDeleteWidgetSceneProxies,
            m_PendingDeleteOldestMs,
            m_bRegisteredForFrameEndDelete);
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
            // The identity token is cleared where the removal runs, not on the game role: nothing
            // outside the render role should be rewriting the state its commands act on.
            widgetComponent->DetachSceneProxy();
            EnqueueRenderCommand([this, oldProxy]() {
                oldProxy->m_WidgetComponent = nullptr;
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
        // The draw lists only: an owner leaves this scene through a removal command, which parks it
        // for the frame-end delete instead of destroying it here. Dropping an owner in this sweep
        // would destroy a proxy a recorded draw command may still name.
        m_PrimitiveSceneProxies.erase(
            std::remove_if(
                m_PrimitiveSceneProxies.begin(),
                m_PrimitiveSceneProxies.end(),
                [](PrimitiveSceneProxy* proxy)
                {
                    return (proxy == nullptr) || (proxy->m_PrimitiveComponent == nullptr) ||
                        proxy->m_bPendingRemove;
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
    }

    void RenderScene::FlushPendingProxyDeletes()
    {
        FrameEndDeleteRegistry& registry = FrameEndDeleteRegistry::Get();

        if (FrameThreading::HasInstance() && !FrameThreading::Get().IsInRenderingThread())
        {
            const uint64_t refusals = registry.Refusals.fetch_add(1, std::memory_order_relaxed) + 1;
            if (refusals == 1 || refusals % 100 == 0)
            {
                ME_LOG(
                    LogRender,
                    Error,
                    "RenderScene::FlushPendingProxyDeletes refused off the render role ({} "
                    "refusal(s)): a scene proxy is destroyed only where the frame that recorded "
                    "against it ends, so the parked proxies stay alive until then.",
                    refusals);
            }
            return;
        }

        std::vector<RenderScene*> scenes;
        {
            std::lock_guard<std::mutex> lock(registry.Mutex);
            scenes.swap(registry.Scenes);
        }

        const uint64_t nowMs = NowMilliseconds();
        for (RenderScene* scene : scenes)
        {
            if (scene == nullptr)
            {
                continue;
            }
            scene->m_bRegisteredForFrameEndDelete = false;

            if (scene->m_PendingDeleteOldestMs != 0 && nowMs > scene->m_PendingDeleteOldestMs)
            {
                const uint64_t waitedMs = nowMs - scene->m_PendingDeleteOldestMs;
                if (waitedMs > kPendingDeleteStallWarnMs)
                {
                    ME_LOG(
                        LogRender,
                        Warn,
                        "RenderScene: a parked proxy waited {} ms for the frame end; frames are "
                        "not reaching their end, so the delete point is running late.",
                        waitedMs);
                }
            }
            scene->m_PendingDeleteOldestMs = 0;

            // Every proxy in these containers got here through a removal, and a removal is what
            // takes a proxy off the draw lists before parking it. Anything else would mean a later
            // command of this frame could still be built from a proxy that is about to be freed,
            // which is exactly the window the two-step delete exists to close.
            const auto bStillDrawable = [](const auto& drawList, const auto* proxy)
            {
                return std::find(drawList.begin(), drawList.end(), proxy) != drawList.end();
            };
            for (const std::unique_ptr<PrimitiveSceneProxy>& parked :
                 scene->m_PendingDeletePrimitiveSceneProxies)
            {
                if (!parked)
                {
                    continue;
                }
                ME_ASSERT(
                    parked->m_bPendingRemove,
                    "a parked primitive proxy carries the removal mark before the frame end frees it");
                ME_ASSERT(
                    !bStillDrawable(scene->m_PrimitiveSceneProxies, parked.get()),
                    "a parked primitive proxy is off the draw list before the frame end frees it");
            }
            for (const std::unique_ptr<WidgetSceneProxy>& parked :
                 scene->m_PendingDeleteWidgetSceneProxies)
            {
                if (!parked)
                {
                    continue;
                }
                ME_ASSERT(
                    !bStillDrawable(scene->m_WidgetSceneProxies, parked.get()),
                    "a parked widget proxy is off the draw list before the frame end frees it");
            }
            for (const std::unique_ptr<LightSceneProxy>& parked :
                 scene->m_PendingDeleteLightSceneProxies)
            {
                if (!parked)
                {
                    continue;
                }
                ME_ASSERT(
                    !bStillDrawable(scene->m_DirectionalLightSceneProxies, parked.get()) &&
                        !bStillDrawable(scene->m_PointLightSceneProxies, parked.get()) &&
                        !bStillDrawable(scene->m_SpotLightSceneProxies, parked.get()),
                    "a parked light proxy is off the draw list before the frame end frees it");
            }
            ME_ASSERT(
                scene->m_PendingDeleteSkyBoxSceneProxy == nullptr ||
                    scene->m_SkyBoxProxy != scene->m_PendingDeleteSkyBoxSceneProxy.get(),
                "a parked skybox proxy is off the draw list before the frame end frees it");

            scene->m_ProxyDeleteCount +=
                scene->m_PendingDeletePrimitiveSceneProxies.size() +
                scene->m_PendingDeleteWidgetSceneProxies.size() +
                scene->m_PendingDeleteLightSceneProxies.size() +
                (scene->m_PendingDeleteSkyBoxSceneProxy ? 1u : 0u);

            // The delete itself, and the whole of it.
            scene->m_PendingDeletePrimitiveSceneProxies.clear();
            scene->m_PendingDeleteWidgetSceneProxies.clear();
            scene->m_PendingDeleteLightSceneProxies.clear();
            scene->m_PendingDeleteSkyBoxSceneProxy.reset();
        }

        // Scenes whose owning Scene is gone. The apply above has just taken their removals off the
        // draw lists and parked them, so a scene that reports nothing left has nothing to wait for
        // and its last reference is dropped here, at the boundary that may free a proxy. One that
        // still owns a proxy is waiting for a removal command of a later frame; freeing it now
        // would pull the scene out from under that command.
        std::vector<std::shared_ptr<RenderScene>> orphanedScenes;
        {
            std::lock_guard<std::mutex> lock(registry.Mutex);
            orphanedScenes.swap(registry.OrphanedScenes);
        }

        std::vector<std::shared_ptr<RenderScene>> stillOwnerless;
        for (std::shared_ptr<RenderScene>& orphanedScene : orphanedScenes)
        {
            if (orphanedScene && orphanedScene->HasPendingProxyWork())
            {
                stillOwnerless.push_back(std::move(orphanedScene));
            }
        }

        if (!stillOwnerless.empty())
        {
            std::lock_guard<std::mutex> lock(registry.Mutex);
            registry.OrphanedScenes.insert(
                registry.OrphanedScenes.begin(),
                std::make_move_iterator(stillOwnerless.begin()),
                std::make_move_iterator(stillOwnerless.end()));
        }
    }

    void DiscardOrphanedRenderScenesForTeardown()
    {
        FrameEndDeleteRegistry& registry = FrameEndDeleteRegistry::Get();

        std::vector<std::shared_ptr<RenderScene>> orphanedScenes;
        {
            std::lock_guard<std::mutex> lock(registry.Mutex);
            orphanedScenes.swap(registry.OrphanedScenes);
        }

        for (std::shared_ptr<RenderScene>& orphanedScene : orphanedScenes)
        {
            if (orphanedScene)
            {
                DiscardRenderSceneProxiesForTeardown(*orphanedScene);
            }
        }
    }
}
