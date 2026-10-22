#pragma once

#include "Runtime/Function/Render/RHI/RHISampler.h"

#include <string>

namespace minEngine
{
    /** Header version stamped into every .metex this build writes. Version 2 added the
        sampler filter / address bytes; version 1 files load as Linear + Repeat. */
    inline constexpr int kMetexVersion = 2;

    struct Texture2DCookSettings
    {
        std::string ChannelSemantics = "Color";
        std::string CookedFormat = "RGB8";
        bool bSRGB = true;
        RHISamplerFilter SamplerFilter = RHISamplerFilter::Linear;
        RHISamplerAddress SamplerAddress = RHISamplerAddress::Repeat;
    };
}
