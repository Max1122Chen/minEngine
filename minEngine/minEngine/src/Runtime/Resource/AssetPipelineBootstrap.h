#pragma once

#include "Core.h"

namespace minEngine
{
    class AssetManager;

    /** Registers builtin LoadHandlers, SaveHandlers, CreateHandlers, and ImportProducts on AssetManager. */
    void RegisterAllAssetPipelines(AssetManager& assetManager);
}
