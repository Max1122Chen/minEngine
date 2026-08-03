#include "ShadowResourceManager.h"

#include "Render/LightSceneProxies/LightSceneProxy.h"
#include "Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Render/LightSceneProxies/SpotLightSceneProxy.h"

namespace minEngine
{
    void ShadowResourceManager::Initialize()
    {
        m_FrameIndex = 0;
    }

    void ShadowResourceManager::Shutdown()
    {
        m_SpotShadowSlots.clear();
        m_PointShadowSlots.clear();
    }

    void ShadowResourceManager::BeginFrame(uint64_t frameIndex)
    {
        m_FrameIndex = frameIndex;
    }

    void ShadowResourceManager::EndFrame()
    {
        (void)m_FrameIndex;
    }

    ShadowResourceHandle ShadowResourceManager::AcquireDirectional(const ShadowRequest& req, uint32_t cascadeCount)
    {
        if (!req.Resolution.IsValid() || cascadeCount == 0)
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::Depth2DArray;
        handle.TextureUnit = m_DirectionalTextureUnit;
        handle.ArrayBaseLayer = 0;
        handle.LayerCount = static_cast<int>(cascadeCount);
        handle.Resolution = req.Resolution;
        handle.Texture = nullptr;
        return handle;
    }

    ShadowResourceHandle ShadowResourceManager::AcquireSpot(const ShadowRequest& req)
    {
        auto* lightProxy = static_cast<SpotLightSceneProxy*>(req.LightProxy);
        if (!req.Resolution.IsValid() || !lightProxy)
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        auto it = m_SpotShadowSlots.find(lightProxy);
        if (it == m_SpotShadowSlots.end())
        {
            if (m_SpotShadowSlots.size() >= static_cast<size_t>(MAX_SPOT_SHADOW_MAPS))
            {
                return ShadowResourceHandle::InvalidHandle();
            }

            SpotShadowSlot slot{};
            slot.TextureUnit = SPOT_SHADOW_MAP_BASE_UNIT + static_cast<int>(m_SpotShadowSlots.size());
            it = m_SpotShadowSlots.emplace(lightProxy, std::move(slot)).first;
        }

        it->second.Resolution = req.Resolution;

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::Depth2D;
        handle.Texture = nullptr;
        handle.TextureUnit = it->second.TextureUnit;
        handle.Resolution = it->second.Resolution;
        return handle;
    }

    ShadowResourceHandle ShadowResourceManager::AcquirePoint(const ShadowRequest& req)
    {
        auto* lightProxy = static_cast<PointLightSceneProxy*>(req.LightProxy);
        if (!req.Resolution.IsValid() || !lightProxy)
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        auto it = m_PointShadowSlots.find(lightProxy);
        if (it == m_PointShadowSlots.end())
        {
            if (m_PointShadowSlots.size() >= static_cast<size_t>(MAX_POINT_SHADOW_MAPS))
            {
                return ShadowResourceHandle::InvalidHandle();
            }

            PointShadowSlot slot{};
            slot.TextureUnit = POINT_SHADOW_MAP_BASE_UNIT + static_cast<int>(m_PointShadowSlots.size());
            it = m_PointShadowSlots.emplace(lightProxy, std::move(slot)).first;
        }

        it->second.Resolution = req.Resolution;

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::DepthCube;
        handle.Texture = nullptr;
        handle.TextureUnit = it->second.TextureUnit;
        handle.Resolution = it->second.Resolution;
        return handle;
    }
}
