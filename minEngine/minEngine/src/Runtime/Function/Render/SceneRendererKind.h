#pragma once

#include "Core.h"

namespace minEngine
{
    enum class SceneRendererKind : uint8_t
    {
        Forward,
        /** RND-F13 diagnostic path: manual pass order, no RenderGraph. */
        Manual,
    };
}
