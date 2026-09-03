#include "Runtime/Function/Animation/Pose.h"

#include "Runtime/Core/Log/LogSystem.h"

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
}
