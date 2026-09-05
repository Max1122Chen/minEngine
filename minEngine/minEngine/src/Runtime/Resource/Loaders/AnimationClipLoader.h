#pragma once

#include "Core.h"
#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Skeleton;

    class AnimationClipLoader
    {
    public:
        // Assimp import → in-memory clip (does not write disk). animationIndex selects scene.mAnimations[i].
        static bool ImportFromFile(
            const std::string& path,
            const std::shared_ptr<Skeleton>& skeleton,
            int animationIndex,
            AnimationClip& outClip,
            std::string* outError = nullptr);

        static bool Save(const AssetMeta& meta, const AnimationClip& clip, std::string* outError = nullptr);
        static std::shared_ptr<AnimationClip> Load(const AssetMeta& meta, std::string* outError = nullptr);

        static int CountAnimationsInFile(const std::string& path, std::string* outError = nullptr);
    };
}
