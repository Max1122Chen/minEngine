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

        // Local TRS blend: Position/Scale lerp, Rotation slerp. Requires equal bone counts.
        // alpha is clamped to [0,1]. Returns false if bone counts differ (including empty mismatch).
        static bool Blend(const Pose& a, const Pose& b, float alpha, Pose& outPose);
    };
}
