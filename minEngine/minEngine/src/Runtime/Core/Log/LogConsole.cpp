#include "LogConsole.h"
#include "LogSystem.h"

#include <cstdio>

namespace minEngine
{
    std::mutex LogConsoleStorage::s_Mutex;
    std::vector<LogConsoleEntry> LogConsoleStorage::s_Entries;

    void LogConsoleStorage::Push(LogConsoleEntry entry)
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Entries.size() >= kMaxEntries)
        {
            s_Entries.erase(s_Entries.begin());
        }
        s_Entries.emplace_back(std::move(entry));
    }

    std::vector<LogConsoleEntry> LogConsoleStorage::Snapshot()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        return s_Entries;
    }

    void LogConsoleStorage::Clear()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Entries.clear();
    }

    void LogConsoleSink::sink_it_(const spdlog::details::log_msg& msg)
    {
        LogConsoleEntry entry;
        entry.timestamp = FormatTimestamp(msg.time);
        entry.loggerName = std::string(msg.logger_name.data(), msg.logger_name.size());
        entry.message = std::string(msg.payload.data(), msg.payload.size());
        entry.level = ConvertLogLevel(msg.level);
        entry.source = DeduceLogSource(entry.loggerName);

        LogConsoleStorage::Push(std::move(entry));
    }

    LogLevel::Level LogConsoleSink::ConvertLogLevel(spdlog::level::level_enum spdlogLevel)
    {
        using spdlog::level::level_enum;
        switch(spdlogLevel)
        {
            case level_enum::trace:     return LogLevel::Level::Trace;
            case level_enum::debug:     return LogLevel::Level::Debug;
            case level_enum::info:      return LogLevel::Level::Info;
            case level_enum::warn:      return LogLevel::Level::Warn;
            case level_enum::err:       return LogLevel::Level::Error;
            case level_enum::critical:  return LogLevel::Level::Critical;
            default:                    return LogLevel::Level::Info;
        }
    }

    LogSource LogConsoleSink::DeduceLogSource(const std::string &loggerName)
    {
        if (loggerName == LogChannelNames::Core)
        {
            return LogSource::Core;
        }
        if (loggerName == LogChannelNames::Client)
        {
            return LogSource::Client;
        }
        return LogSource::Unknown;
    }

    std::string LogConsoleSink::FormatTimestamp(const spdlog::log_clock::time_point& tp)
    {
        const std::time_t tt = spdlog::log_clock::to_time_t(tp);
        std::tm localTm = {};
#ifdef _WIN32
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif

        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
        return std::string(buffer);
    }
}
