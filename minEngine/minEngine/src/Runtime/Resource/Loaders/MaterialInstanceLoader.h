#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class MaterialInstance;

    class MaterialInstanceLoader
    {
    public:
        /** Deserialize .memi and finalize Parent/proxy. Does not compile. */
        static std::shared_ptr<MaterialInstance> Load(const AssetMeta& meta, std::string* outError = nullptr);
    };
}
