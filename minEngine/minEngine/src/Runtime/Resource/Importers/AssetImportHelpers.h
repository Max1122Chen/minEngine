#pragma once

#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    class AssetImportHelpers
    {
    public:
        static ImportResult MakeResultFromLegacy(const ImportAssetResult& legacy);
    };
}
