#pragma once

#include "Render/RHI/RHIShaderBinding.h"

#include <cstdint>
#include <unordered_map>

namespace minEngine
{
    class RHICommandList;
    class RHITexture;

    /** Flyweight cache for texture SRVs (RND-F04-S04). Keyed by texture + array slice. */
    class RHITextureViewCache
    {
    public:
        RHIShaderResourceViewRef GetOrCreate(RHICommandList& cmdList, RHITexture* texture, int32_t arraySlice = -1);
        void Clear();

    private:
        struct Key
        {
            RHITexture* Texture = nullptr;
            int32_t ArraySlice = -1;

            bool operator==(const Key& other) const
            {
                return Texture == other.Texture && ArraySlice == other.ArraySlice;
            }
        };

        struct KeyHash
        {
            size_t operator()(const Key& key) const
            {
                const size_t textureHash = std::hash<RHITexture*>()(key.Texture);
                const size_t sliceHash = std::hash<int32_t>()(key.ArraySlice);
                return textureHash ^ (sliceHash + 0x9e3779b9 + (textureHash << 6) + (textureHash >> 2));
            }
        };

        std::unordered_map<Key, RHIShaderResourceViewRef, KeyHash> m_Cache;
    };
}
