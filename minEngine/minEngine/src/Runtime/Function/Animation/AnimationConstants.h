#pragma once

#include "Core.h"

#include <cstdint>

namespace minEngine
{
    enum class MeshDeformationMode : uint8_t
    {
        Rigid = 0,
        Skinned = 1,
    };

    // Import and palette limits.
    constexpr int32_t kMaxBoneInfluences = 4;
    constexpr int32_t kMaxBonesPerSkeleton = 256;
}
