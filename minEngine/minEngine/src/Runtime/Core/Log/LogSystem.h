#pragma once

#include "EngineAPI.h"
#include "LogChannel.h"
#include "LogRecord.h"

#include "spdlog/fmt/fmt.h"

#include <chrono>
#include <filesystem>
#include <functional>
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

        /** Directory for the session file. Empty uses <process exe>/Logs. Ignored after Initialize. */
        static void SetSessionLogDirectory(const std::filesystem::path& directory);
        static std::filesystem::path GetSessionLogPath();
        static std::filesystem::path GetSessionPointerPath();
        /** Writes Saved/Logs/latest-session-<exe-stem>.txt under projectRoot. False if no session file or the write fails. */
        static bool PublishSessionPointer(const std::filesystem::path& projectRoot);

        static std::string FormatLocalTimestamp(std::chrono::system_clock::time_point timePoint);

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

        /** Snapshot of registered channels (stable after static init in normal Editor use). */
        static void ForEachRegisteredChannel(const std::function<void(LogChannelBase&)>& fn);

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
