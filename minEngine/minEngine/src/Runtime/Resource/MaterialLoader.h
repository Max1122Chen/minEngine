#pragma once

#include "Core.h"
#include "AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Material;

    /** Disk IO + graph finalize for .memtl (no GPU compile). */
    class MaterialLoader
    {
    public:
        static std::shared_ptr<Material> LoadDeserialized(const AssetMeta& meta, std::string* outError = nullptr);
    };
}
