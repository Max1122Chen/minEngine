#pragma once

#include "Core.h"
#include "Runtime/Function/Animation/Skeleton.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class SkeletonLoader
    {
    public:
        static std::shared_ptr<Skeleton> Load(const AssetMeta& meta, std::string* outError = nullptr);
        static bool Save(const AssetMeta& meta, const Skeleton& skeleton, std::string* outError = nullptr);
    };
}
