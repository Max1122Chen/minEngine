#pragma once

#include "Core.h"
#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class AnimationClipLoader
    {
    public:
        static bool Save(const AssetMeta& meta, const AnimationClip& clip, std::string* outError = nullptr);
        static std::shared_ptr<AnimationClip> Load(const AssetMeta& meta, std::string* outError = nullptr);
    };
}
