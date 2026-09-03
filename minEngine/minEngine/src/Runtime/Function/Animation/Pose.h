#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace minEngine
{
    struct Pose
    {
        std::vector<Transform> LocalTransforms;

        void ResetToIdentity(size_t boneCount);
        void Resize(size_t boneCount);

        Transform& At(int32_t boneIndex);
        const Transform& At(int32_t boneIndex) const;

        int32_t GetBoneCount() const { return static_cast<int32_t>(LocalTransforms.size()); }
    };
}
