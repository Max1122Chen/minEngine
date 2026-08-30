#pragma once

#include "Core.h"

namespace minEngine
{
    constexpr uint32_t MAX_POINT_LIGHTS = 16;
    constexpr uint32_t MAX_SPOT_LIGHTS = 16;
    /** BUG-RENDER-010 isolation: single cascade (restore 4 after verify). */
    constexpr uint32_t MAX_CASCADES = 1;
    constexpr uint32_t kShadowMapResolution = 1024;
}
