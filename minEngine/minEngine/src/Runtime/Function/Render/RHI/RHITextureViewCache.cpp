#include "RHITextureViewCache.h"

#include "Runtime/Function/Render/RHI/RHICommandList.h"

namespace minEngine
{
    RHIShaderResourceViewRef RHITextureViewCache::GetOrCreate(
        RHICommandList& cmdList,
        RHITexture* texture,
        int32_t arraySlice)
    {
        if (!texture)
        {
            return nullptr;
        }

        const Key key{texture, arraySlice};
        const auto existing = m_Cache.find(key);
        if (existing != m_Cache.end())
        {
            return existing->second;
        }

        RHITextureSRVDesc srvDesc;
        srvDesc.Texture = texture;
        srvDesc.ArraySlice = arraySlice;
        RHIShaderResourceViewRef srv = cmdList.CreateShaderResourceView(srvDesc);
        if (srv)
        {
            m_Cache.emplace(key, srv);
        }
        return srv;
    }

    void RHITextureViewCache::Clear()
    {
        m_Cache.clear();
    }
}
