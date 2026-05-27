#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Material;

    class MaterialLoader
    {
    public:
        /** Deserialize .memtl, finalize graph, compile GPU material. Returns nullptr on any failure. */
        static std::shared_ptr<Material> Load(const AssetMeta& meta, std::string* outError = nullptr);
    };
}
