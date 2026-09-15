#include "Runtime/Core/Profiling/ProfileClock.h"

#include <chrono>

namespace minEngine::Profile
{
    ProfileTimestamp ProfileClock::Now()
    {
        using Clock = std::chrono::steady_clock;
        const auto now = Clock::now().time_since_epoch();
        return static_cast<ProfileTimestamp>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }

    double ProfileClock::ToSeconds(ProfileTimestamp delta)
    {
        return static_cast<double>(delta) / 1'000'000'000.0;
    }

    double ProfileClock::ToMilliseconds(ProfileTimestamp delta)
    {
        return static_cast<double>(delta) / 1'000'000.0;
    }

    int64_t ProfileClock::ToMicroseconds(ProfileTimestamp delta)
    {
        return static_cast<int64_t>(delta / 1000u);
    }
}
