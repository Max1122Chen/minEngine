#include "Runtime/Function/Animation/Skeleton.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <utility>

namespace minEngine
{
    const SkeletonBone& Skeleton::GetBone(int32_t index) const
    {
        ME_ASSERT(
            index >= 0 && index < GetBoneCount(),
            "Skeleton::GetBone: index out of range");
        return m_Bones[static_cast<size_t>(index)];
    }

    int32_t Skeleton::FindBoneIndex(std::string_view name) const
    {
        for (int32_t boneIndex = 0; boneIndex < GetBoneCount(); ++boneIndex)
        {
            if (m_Bones[static_cast<size_t>(boneIndex)].Name == name)
            {
                return boneIndex;
            }
        }
        return -1;
    }

    bool Skeleton::ValidateBones(const std::vector<SkeletonBone>& bones, std::string* outError) const
    {
        if (bones.size() > static_cast<size_t>(kMaxBonesPerSkeleton))
        {
            if (outError != nullptr)
            {
                *outError = "Skeleton bone count exceeds kMaxBonesPerSkeleton";
            }
            return false;
        }

        const int32_t boneCount = static_cast<int32_t>(bones.size());
        for (int32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            const int32_t parentIndex = bones[static_cast<size_t>(boneIndex)].ParentIndex;
            if (parentIndex < -1 || parentIndex >= boneCount)
            {
                if (outError != nullptr)
                {
                    *outError = "Skeleton bone ParentIndex out of range";
                }
                return false;
            }
            if (parentIndex >= boneIndex)
            {
                // Parents must appear before children so LocalToGlobal is a single forward pass.
                if (outError != nullptr)
                {
                    *outError = "Skeleton bones must be ordered so each parent appears before its children";
                }
                return false;
            }
        }
        return true;
    }

    bool Skeleton::SetBones(std::vector<SkeletonBone> bones, std::string* outError)
    {
        if (!ValidateBones(bones, outError))
        {
            return false;
        }

        m_Bones = std::move(bones);
        return true;
    }

    void Skeleton::FillBindPose(Pose& outPose) const
    {
        const int32_t boneCount = GetBoneCount();
        outPose.Resize(static_cast<size_t>(boneCount));
        for (int32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            outPose.At(boneIndex) = m_Bones[static_cast<size_t>(boneIndex)].LocalBind;
        }
    }

    void Skeleton::LocalToGlobal(const Pose& localPose, std::vector<Matrix4>& outGlobal) const
    {
        const int32_t boneCount = GetBoneCount();
        ME_ASSERT(
            localPose.GetBoneCount() == boneCount,
            "Skeleton::LocalToGlobal: Pose bone count mismatch");

        outGlobal.resize(static_cast<size_t>(boneCount));
        for (int32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            const Matrix4 localMatrix = localPose.At(boneIndex).ToMatrix();
            const int32_t parentIndex = m_Bones[static_cast<size_t>(boneIndex)].ParentIndex;
            if (parentIndex < 0)
            {
                outGlobal[static_cast<size_t>(boneIndex)] = localMatrix;
            }
            else
            {
                outGlobal[static_cast<size_t>(boneIndex)] =
                    outGlobal[static_cast<size_t>(parentIndex)] * localMatrix;
            }
        }
    }

    void Skeleton::BuildSkinningPalette(const Pose& localPose, std::vector<Matrix4>& outPalette) const
    {
        std::vector<Matrix4> globalPose;
        LocalToGlobal(localPose, globalPose);

        const int32_t boneCount = GetBoneCount();
        outPalette.resize(static_cast<size_t>(boneCount));
        for (int32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            outPalette[static_cast<size_t>(boneIndex)] =
                globalPose[static_cast<size_t>(boneIndex)]
                * m_Bones[static_cast<size_t>(boneIndex)].InverseBindPose;
        }
    }
}
