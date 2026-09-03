#pragma once

#include "Core.h"
#include "Runtime/Function/Animation/AnimationConstants.h"
#include "Runtime/Function/Animation/Pose.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Resource/Asset.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    struct SkeletonBone
    {
        std::string Name;
        int32_t ParentIndex = -1;
        Transform LocalBind{};
        Matrix4 InverseBindPose{1.0f};
    };

    ME_CLASS()
    class Skeleton : public Asset
    {
        ME_GENERATED_BODY(Skeleton)
    public:
        Skeleton() = default;
        ~Skeleton() override = default;

        int32_t GetBoneCount() const { return static_cast<int32_t>(m_Bones.size()); }
        const SkeletonBone& GetBone(int32_t index) const;
        int32_t FindBoneIndex(std::string_view name) const;

        // Replaces bone table. Returns false if hierarchy/count invalid.
        bool SetBones(std::vector<SkeletonBone> bones, std::string* outError = nullptr);

        void FillBindPose(Pose& outPose) const;
        void LocalToGlobal(const Pose& localPose, std::vector<Matrix4>& outGlobal) const;
        void BuildSkinningPalette(const Pose& localPose, std::vector<Matrix4>& outPalette) const;

    private:
        bool ValidateBones(const std::vector<SkeletonBone>& bones, std::string* outError) const;

        std::vector<SkeletonBone> m_Bones;
    };
}

#include "Generated/Reflection/Skeleton.gen.h"
