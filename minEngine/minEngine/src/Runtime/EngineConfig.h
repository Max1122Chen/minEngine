#pragma once
#include "Core.h"

namespace minEngine
{
    ME_CLASS()
    struct EngineConfig
    {
        ME_GENERATED_BODY(EngineConfig)

        ME_PROPERTY()
        std::string EngineDefaultAssetsRoot;
    };
}

#include "EngineConfig.gen.h"