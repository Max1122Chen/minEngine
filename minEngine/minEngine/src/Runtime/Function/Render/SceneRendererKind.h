#pragma once

#include "Core.h"

namespace minEngine
{
    enum class SceneRendererKind : uint8_t
    {
        Forward,
        /** diagnostic path: manual pass order, no RenderGraph. */
        Manual,
    };
}
