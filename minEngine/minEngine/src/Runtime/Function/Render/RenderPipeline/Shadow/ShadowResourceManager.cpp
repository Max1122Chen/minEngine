#include "ShadowResourceManager.h"

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

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
        handle.TextureUnit = m_DirectionalConfig.TextureUnit;
        handle.ArrayBaseLayer = 0;
        handle.LayerCount = static_cast<int>(m_DirectionalConfig.Layers);
        handle.CubeIndex = -1;
        handle.Resolution = m_DirectionalConfig.Resolution;
        handle.Valid = true;
        return handle;
    }

    ShadowResourceHandle ShadowResourceManager::AcquireSpot(const ShadowRequest& req)
    {
        (void)req;
        // Interface placeholder: future spot shadow atlas allocation.
        return ShadowResourceHandle::InvalidHandle();
    }

    ShadowResourceHandle ShadowResourceManager::AcquirePoint(const ShadowRequest& req)
    {
        (void)req;
        // Interface placeholder: future point light cube shadow allocation.
        return ShadowResourceHandle::InvalidHandle();
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
}
