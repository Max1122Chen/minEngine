#pragma once

#include <cstdint>

namespace minEngine
{
    struct BackendVoiceHandle
    {
        uint32_t Index{UINT32_MAX};

        bool IsValid() const { return Index != UINT32_MAX; }
    };
}
