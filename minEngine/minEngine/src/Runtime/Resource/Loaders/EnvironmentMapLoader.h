#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class EnvironmentMap;

    class EnvironmentMapLoader
    {
    public:
        static std::shared_ptr<EnvironmentMap> Load(const AssetMeta& meta, std::string* outError = nullptr);
    };
}
