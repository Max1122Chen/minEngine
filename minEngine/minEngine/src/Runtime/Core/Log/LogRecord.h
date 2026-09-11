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
        LogSeverity severity = LogSeverity::Info;
        const LogChannelBase* channel = nullptr;
        std::string channelName;
        std::string message;
        LogSourceLocation source{};
        uint32_t threadId = 0;
        std::vector<LogField> fields;

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
