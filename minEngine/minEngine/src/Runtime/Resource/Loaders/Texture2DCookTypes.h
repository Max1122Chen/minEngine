#pragma once

#include <cstdint>
#include <string>

namespace minEngine
{
    struct Texture2DCookSettings
    {
        int Version = 1;
        std::string ChannelSemantics = "Color";
        std::string CookedFormat = "RGB8";
        bool bSRGB = true;
        std::string WrapU = "Repeat";
        std::string WrapV = "Repeat";
        std::string Filter = "Linear";
    };
}
