#pragma once

#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Resource/AssetManager.h"

#include <memory>
#include <string>

namespace minEngine
{
    class AnimationGraphLoader
    {
    public:
        static bool Save(const AssetMeta& meta, const AnimationGraph& graph, std::string* outError = nullptr);
        static std::shared_ptr<AnimationGraph> Load(const AssetMeta& meta, std::string* outError = nullptr);
    };
}
