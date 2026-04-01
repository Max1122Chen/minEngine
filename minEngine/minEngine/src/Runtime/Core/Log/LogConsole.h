#pragma once

#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/base_sink.h"

#include <mutex>
#include <string>
#include <vector>

namespace minEngine
{
    enum class LogSource : uint8_t
    {
        Core,
        Client,
        Unknown
    };

    struct LogLevel
    {
        enum class Level: uint8_t
        {
            Trace,
            Debug,
            Info,
            Warn,
            Error,
            Critical
        };

        static const char* ToString(Level level)
        {
            switch (level)
            {
                case Level::Trace:      return "Trace";
                case Level::Debug:      return "Debug";
                case Level::Info:       return "Info";
                case Level::Warn:       return "Warn";
                case Level::Error:      return "Error";
                case Level::Critical:   return "Critical";
                default:                return "Unknown";
            }
        }
    };
    

    struct LogConsoleEntry
    {
        std::string timestamp;
        std::string loggerName;
        std::string message;
        LogLevel::Level level = LogLevel::Level::Info;
        LogSource source = LogSource::Unknown;
    };

    class LogConsoleStorage
    {
    public:
        static void Push(LogConsoleEntry entry);
        static std::vector<LogConsoleEntry> Snapshot();
        static void Clear();

    private:
        static constexpr size_t kMaxEntries = 2000;
        static std::mutex s_Mutex;
        static std::vector<LogConsoleEntry> s_Entries;
    };

    class LogConsoleSink : public spdlog::sinks::base_sink<std::mutex>
    {
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override;
        void flush_() override {}

    private:
        static LogLevel::Level ConvertLogLevel(spdlog::level::level_enum spdlogLevel);
        static LogSource DeduceLogSource(const std::string& loggerName);
        static std::string FormatTimestamp(const spdlog::log_clock::time_point& tp);
    };
}
