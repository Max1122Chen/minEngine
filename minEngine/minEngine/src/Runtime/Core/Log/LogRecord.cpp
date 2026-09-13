#include "LogRecord.h"

#include <cstdio>
#include <ctime>

namespace minEngine
{
    void LogRecord::EnsureLocalDisplayTime()
    {
        if (!displayTime.empty())
        {
            return;
        }

        const std::time_t tt = std::chrono::system_clock::to_time_t(timestamp);
        std::tm localTm = {};
#ifdef _WIN32
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
        displayTime.assign(buffer);
    }
}
