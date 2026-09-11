#pragma once

#include "LogChannel.h"
#include "LogRecord.h"

#include "spdlog/fmt/fmt.h"

#include <memory>
#include <string>
#include <string_view>

namespace minEngine
{
    class MINENGINE_API LogSystem
    {
    public:
        static void Initialize();
        static void Shutdown();
        static LogSystem& Get();

        static void AddSink(std::shared_ptr<ILogSink> sink);
        static void Emit(
            const LogChannelBase& channel,
            LogSeverity severity,
            std::string message,
            LogSourceLocation source);
        static void SetChannelSeverity(LogChannelBase& channel, LogSeverity severity);
        static void SetChannelSeverityByName(std::string_view name, LogSeverity severity);
        static void Flush();

        static void RegisterChannel(LogChannelBase& channel);
        static void UnregisterChannel(LogChannelBase& channel);

    private:
        LogSystem() = default;
    };
}

#define ME_LOG(Channel, Severity, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        if (!(Channel).IsSuppressed(::minEngine::LogSeverity::Severity))                           \
        {                                                                                          \
            ::minEngine::LogSystem::Emit(                                                          \
                (Channel),                                                                         \
                ::minEngine::LogSeverity::Severity,                                                \
                ::fmt::format(__VA_ARGS__),                                                        \
                ::minEngine::LogSourceLocation{__FILE__, __LINE__});                               \
        }                                                                                          \
    } while (0)
