#pragma once

#include "LogChannel.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    struct LogSourceLocation
    {
        const char* file = nullptr;
        int line = 0;
    };

    // Placeholder for structured fields (L1 deferred).
    struct LogField
    {
    };

    struct LogRecord
    {
        std::chrono::system_clock::time_point timestamp{};
        // Local wall-clock label filled once (e.g. "HH:MM:SS"); keep timestamp as source of truth.
        std::string displayTime;
        LogSeverity severity = LogSeverity::Info;
        const LogChannelBase* channel = nullptr;
        std::string channelName;
        std::string message;
        LogSourceLocation source{};
        uint32_t threadId = 0;
        std::vector<LogField> fields;

        void EnsureLocalDisplayTime();

        const char* GetChannelName() const
        {
            if (channel != nullptr)
            {
                return channel->GetName();
            }
            return channelName.c_str();
        }
    };

    class ILogSink
    {
    public:
        virtual ~ILogSink() = default;
        virtual void Write(const LogRecord& record) = 0;
        virtual void Flush() {}
    };
}
