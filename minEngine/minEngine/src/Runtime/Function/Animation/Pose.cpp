#include "Runtime/Function/Animation/Pose.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Quaternion.h"

#include <algorithm>
#include <cmath>

namespace minEngine
{
    void Pose::ResetToIdentity(size_t boneCount)
    {
        LocalTransforms.assign(boneCount, Transform{});
    }

    void Pose::Resize(size_t boneCount)
    {
        LocalTransforms.resize(boneCount);
    }

    Transform& Pose::At(int32_t boneIndex)
    {
        ME_ASSERT(
            boneIndex >= 0 && boneIndex < GetBoneCount(),
            "Pose::At: boneIndex out of range");
        return LocalTransforms[static_cast<size_t>(boneIndex)];
    }

    const Transform& Pose::At(int32_t boneIndex) const
    {
        ME_ASSERT(
            boneIndex >= 0 && boneIndex < GetBoneCount(),
            "Pose::At: boneIndex out of range");
        return LocalTransforms[static_cast<size_t>(boneIndex)];
    }

    bool Pose::Blend(const Pose& a, const Pose& b, float alpha, Pose& outPose)
    {
        if (a.GetBoneCount() != b.GetBoneCount())
        {
            return false;
        }

        const float t = std::clamp(alpha, 0.0f, 1.0f);
        const int32_t boneCount = a.GetBoneCount();
        outPose.Resize(static_cast<size_t>(boneCount));

        if (boneCount == 0)
        {
            return true;
        }

        if (t <= 0.0f)
        {
            outPose.LocalTransforms = a.LocalTransforms;
            return true;
        }
        if (t >= 1.0f)
        {
            outPose.LocalTransforms = b.LocalTransforms;
            return true;
        }

        for (int32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            const Transform& from = a.At(boneIndex);
            const Transform& to = b.At(boneIndex);
            Transform& out = outPose.At(boneIndex);

            out.Position = from.Position + (to.Position - from.Position) * t;
            out.Scale = from.Scale + (to.Scale - from.Scale) * t;
            out.Rotation = Quaternion::FromGlm(glm::normalize(
                glm::slerp(from.Rotation.ToGlm(), to.Rotation.ToGlm(), t)));
        }

        return true;
    }
}
