#pragma once

#include "Render/RHI/RHITexture.h"

#include <cstdint>

namespace minEngine
{
    struct RHITextureSubresourceRange
    {
        uint32_t BaseMipLevel = 0;
        uint32_t LevelCount = 1;
    };

    /** Placeholder barrier descriptor; OpenGL backend ignores contents (RND-F04-S04). */
    struct RHITextureTransitionInfo
    {
        RHITexture* Texture = nullptr;
        RHITextureSubresourceRange Subresource = {};
    };
}
