#pragma once

#include "Runtime/Core/Profiling/ProfileTypes.h"

#include "EngineAPI.h"

namespace minEngine::Profile
{
    struct ProfileClock
    {
        static MINENGINE_API ProfileTimestamp Now();
        static MINENGINE_API double ToSeconds(ProfileTimestamp delta);
        static MINENGINE_API double ToMilliseconds(ProfileTimestamp delta);
        static MINENGINE_API int64_t ToMicroseconds(ProfileTimestamp delta);
    };
}
