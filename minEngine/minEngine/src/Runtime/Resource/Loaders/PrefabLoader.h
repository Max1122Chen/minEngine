#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Prefab;

    class PrefabLoader
    {
    public:
        static std::shared_ptr<Prefab> Load(const AssetMeta& meta, std::string* outError = nullptr);
    };
}
