#include "ShadowResourceManager.h"

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Render/LightSceneProxies/LightSceneProxy.h"
#include "Render/LightSceneProxies/SpotLightSceneProxy.h"
#include "Render/LightSceneProxies/PointLightSceneProxy.h"

namespace minEngine
{
    void ShadowResourceManager::Initialize(RHI* rhi)
    {
        m_RHI = rhi;
        m_FrameIndex = 0;
    }

    void ShadowResourceManager::Shutdown()
    {
        m_DirectionalShadowArray.reset();
        m_DirectionalConfig = DirectionalArrayResource{};
        m_SpotShadowResources.clear();
        m_PointShadowResources.clear();
        m_RHI = nullptr;
    }

    void ShadowResourceManager::BeginFrame(uint64_t frameIndex)
    {
        m_FrameIndex = frameIndex;
    }

    void ShadowResourceManager::EndFrame()
    {
        // Reserved for future LRU or aging policy.
    }

    ShadowResourceHandle ShadowResourceManager::AcquireDirectional(const ShadowRequest& req, uint32_t cascadeCount)
    {
        if (!req.Resolution.IsValid() || cascadeCount == 0)
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        if (!EnsureDirectionalResource(req, cascadeCount))
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::Depth2DArray;
        handle.ResourcePtr = m_DirectionalShadowArray;
        handle.TextureUnit = m_DirectionalConfig.TextureUnit;
        handle.ArrayBaseLayer = 0;
        handle.LayerCount = static_cast<int>(m_DirectionalConfig.Layers);
        handle.Resolution = m_DirectionalConfig.Resolution;
        return handle;
    }

    ShadowResourceHandle ShadowResourceManager::AcquireSpot(const ShadowRequest& req)
    {
        auto* lightProxy = static_cast<SpotLightSceneProxy*>(req.LightProxy);
        if (!req.Resolution.IsValid() || !lightProxy)
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        auto it = m_SpotShadowResources.find(lightProxy);
        if (it == m_SpotShadowResources.end())
        {
            if (m_SpotShadowResources.size() >= static_cast<size_t>(MAX_SPOT_SHADOW_MAPS))
            {
                return ShadowResourceHandle::InvalidHandle();
            }

            SpotShadowResource resource{};
            resource.TextureUnit = SPOT_SHADOW_MAP_BASE_UNIT + static_cast<int>(m_SpotShadowResources.size());
            it = m_SpotShadowResources.emplace(lightProxy, std::move(resource)).first;
        }

        if (!EnsureSpotResource(req, it->second))
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::Depth2D;
        handle.ResourcePtr = it->second.Texture;
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

        auto it = m_PointShadowResources.find(lightProxy);
        if (it == m_PointShadowResources.end())
        {
            if (m_PointShadowResources.size() >= static_cast<size_t>(MAX_POINT_SHADOW_MAPS))
            {
                return ShadowResourceHandle::InvalidHandle();
            }

            PointShadowResource resource{};
            resource.TextureUnit = POINT_SHADOW_MAP_BASE_UNIT + static_cast<int>(m_PointShadowResources.size());
            it = m_PointShadowResources.emplace(lightProxy, std::move(resource)).first;
        }

        if (!EnsurePointResource(req, it->second))
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::DepthCube;
        handle.ResourcePtr = it->second.Texture;
        handle.TextureUnit = it->second.TextureUnit;
        handle.Resolution = it->second.Resolution;
        return handle;
    }

    bool ShadowResourceManager::EnsureDirectionalResource(const ShadowRequest& req, uint32_t cascadeCount)
    {
        if (!m_RHI)
        {
            return false;
        }

        const bool needRecreate =
            (m_DirectionalShadowArray == nullptr) ||
            (m_DirectionalConfig.Resolution != req.Resolution) ||
            (m_DirectionalConfig.Layers != cascadeCount);

        if (!needRecreate)
        {
            return true;
        }

        RHITextureDesc desc{};
        desc.Width = req.Resolution.Width;
        desc.Height = req.Resolution.Height;
        desc.Layers = cascadeCount;
        desc.Format = TextureFormat::DEPTH32;
        desc.Usage = TextureUsage::Depth;

        // TODO: asign texture unit based on some allocation strategy if we support more shadow resources in the future.
        auto newArray = m_RHI->CreateRHITexture2DArray(nullptr, desc);
        if (!newArray)
        {
            return false;
        }

        m_DirectionalShadowArray = newArray;
        m_DirectionalConfig.Resolution = req.Resolution;
        m_DirectionalConfig.Layers = cascadeCount;
        return true;
    }

    bool ShadowResourceManager::EnsureSpotResource(const ShadowRequest& req, SpotShadowResource& resource)
    {
        if (!m_RHI)
        {
            return false;
        }

        const bool needRecreate =
            (resource.Texture == nullptr) ||
            (resource.Resolution != req.Resolution);

        if (!needRecreate)
        {
            return true;
        }

        RHITextureDesc desc{};
        desc.Width = req.Resolution.Width;
        desc.Height = req.Resolution.Height;
        desc.Format = TextureFormat::DEPTH32;
        desc.Usage = TextureUsage::Depth;

        auto newTexture = m_RHI->CreateRHITexture2D(nullptr, desc);
        if (!newTexture)
        {
            return false;
        }

        resource.Texture = newTexture;
        resource.Resolution = req.Resolution;
        return true;
    }

    bool ShadowResourceManager::EnsurePointResource(const ShadowRequest& req, PointShadowResource& resource)
    {
        if (!m_RHI)
        {
            return false;
        }

        const bool needRecreate =
            (resource.Texture == nullptr) ||
            (resource.Resolution != req.Resolution);

        if (!needRecreate)
        {
            return true;
        }

        RHITextureDesc desc{};
        desc.Width = req.Resolution.Width;
        desc.Height = req.Resolution.Height;
        desc.Format = TextureFormat::DEPTH32;
        desc.Usage = TextureUsage::Depth;

        std::vector<unsigned char*> emptyFaces(6, nullptr);
        auto newCube = m_RHI->CreateRHITextureCube(emptyFaces, desc);
        if (!newCube)
        {
            return false;
        }

        resource.Texture = newCube;
        resource.Resolution = req.Resolution;
        return true;
    }
}
