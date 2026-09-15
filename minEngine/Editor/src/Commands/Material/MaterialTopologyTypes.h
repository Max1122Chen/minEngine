#pragma once

#include <cstdint>

namespace minEngine
{
    struct MaterialNodeInboundLink
    {
        uint64_t ToNodeDefHigh = 0;
        uint64_t ToNodeDefLow = 0;
        int32_t ToInputIndex = 0;
        int32_t FromOutputIndex = 0;
    };
}
