#pragma once

#include "Core.h"

namespace minEngine
{
    /** Per-pass CPU state container base (extended by concrete passes in S02+). */
    struct PassParameters
    {
        virtual ~PassParameters() = default;
    };
}
