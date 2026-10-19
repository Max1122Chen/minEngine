#include "LogSystem.h"

#include "LogConsole.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace minEngine
{
    // File-scope implementation state (avoids static-init order issues with LogSystem).
    class LogSystemInternals
    {
    public:
        static LogSystemInternals& Get()
        {
            static LogSystemInternals instance;
            return instance;
        }

        std::mutex mutex;
        std::vector<std::shared_ptr<ILogSink>> sinks;
        std::vector<LogChannelBase*> channels;
        bool initialized = false;
        std::filesystem::path sessionDirectoryOverride;
        std::filesystem::path sessionLogPath;
        std::filesystem::path sessionPointerPath;
        std::string executableStem;

        static uint32_t CurrentThreadId()
        {
            const auto id = std::this_thread::get_id();
            return static_cast<uint32_t>(std::hash<std::thread::id>{}(id));
        }

        static std::filesystem::path ExecutablePath()
        {
#if defined(_WIN32)
            std::wstring buffer(32768, L'\0');
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0 || length >= buffer.size())
            {
                return {};
            }
            buffer.resize(length);
            return std::filesystem::path(buffer);
#else
            return {};
#endif
        }

        static void PruneSessionFiles(const std::filesystem::path& directory)
        {
            namespace fs = std::filesystem;
            std::vector<fs::directory_entry> files;
            std::error_code iterateError;
            for (fs::directory_iterator it(directory, iterateError), end; !iterateError && it != end; it.increment(iterateError))
            {
                const fs::directory_entry& entry = *it;
                std::error_code statusError;
                if (!entry.is_regular_file(statusError) || statusError)
                {
                    continue;
                }
                const std::string name = entry.path().filename().string();
                if (name.rfind("minEngine-", 0) == 0 && entry.path().extension() == ".log")
                {
                    files.push_back(entry);
                }
            }

            constexpr size_t kKeepBeforeNewFile = 19;
            if (files.size() <= kKeepBeforeNewFile)
            {
                return;
            }

            std::sort(files.begin(), files.end(), [](const fs::directory_entry& left, const fs::directory_entry& right) {
                std::error_code leftError;
                std::error_code rightError;
                const auto leftTime = left.last_write_time(leftError);
                const auto rightTime = right.last_write_time(rightError);
                if (leftError || rightError)
                {
                    return left.path().filename().string() < right.path().filename().string();
                }
                return leftTime < rightTime;
            });

            while (files.size() > kKeepBeforeNewFile)
            {
                std::error_code removeError;
                fs::remove(files.front().path(), removeError);
                files.erase(files.begin());
            }
        }

        static std::string MakeSessionFileName()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
            std::tm localTime{};
#if defined(_WIN32)
            localtime_s(&localTime, &seconds);
            const unsigned long pid = GetCurrentProcessId();
#else
            localtime_r(&seconds, &localTime);
            const unsigned long pid = static_cast<unsigned long>(getpid());
#endif
            const auto millis =
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            int millisecond = static_cast<int>(millis.count());
            if (millisecond < 0)
            {
                millisecond = 0;
            }

            return fmt::format(
                "minEngine-{:04}{:02}{:02}-{:02}{:02}{:02}-{:03}-{}.log",
                localTime.tm_year + 1900,
                localTime.tm_mon + 1,
                localTime.tm_mday,
                localTime.tm_hour,
                localTime.tm_min,
                localTime.tm_sec,
                millisecond,
                pid);
        }

        static std::string FormatFileLine(const LogRecord& record)
        {
            std::string line = fmt::format(
                "[{}] [{}] [{}] {}",
                LogSystem::FormatLocalTimestamp(record.timestamp),
                record.GetChannelName(),
                LogChannelBase::SeverityToString(record.severity),
                record.message);
            if (record.source.file != nullptr && record.source.file[0] != '\0')
            {
                line += fmt::format(" ({}:{})", record.source.file, record.source.line);
            }
            return line;
        }

        bool TryOpenSessionFile()
        {
            namespace fs = std::filesystem;
            sessionLogPath.clear();

            fs::path directory = sessionDirectoryOverride;
            if (directory.empty())
            {
                const fs::path executable = ExecutablePath();
                if (executable.empty())
                {
                    return false;
                }
                executableStem = executable.stem().string();
                directory = executable.parent_path() / "Logs";
            }
            else
            {
                const fs::path executable = ExecutablePath();
                executableStem = executable.empty() ? "process" : executable.stem().string();
            }

            if (executableStem.empty())
            {
                executableStem = "process";
            }

            std::error_code createError;
            fs::create_directories(directory, createError);
            if (createError)
            {
                return false;
            }

            PruneSessionFiles(directory);

            const fs::path filePath = directory / MakeSessionFileName();
            try
            {
                auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath.string(), true);
                auto fileLogger = std::make_shared<spdlog::logger>("minEngine.file", std::move(fileSink));
                fileLogger->set_level(spdlog::level::trace);
                fileLogger->set_pattern("%v");
                fileLogger->flush_on(spdlog::level::warn);
                spdlog::register_logger(fileLogger);
                sessionLogPath = filePath;
                return true;
            }
            catch (const std::exception&)
            {
                sessionLogPath.clear();
                return false;
            }
        }

    private:
        LogSystemInternals() = default;
    };

    class SpdlogTextSink final : public ILogSink
    {
    public:
        SpdlogTextSink(std::shared_ptr<spdlog::logger> logger, bool fullLine)
            : m_Logger(std::move(logger))
            , m_FullLine(fullLine)
        {
        }

        void Write(const LogRecord& record) override
        {
            if (m_Logger == nullptr)
            {
                return;
            }

            if (m_FullLine)
            {
                m_Logger->log(ToSpdlogLevel(record.severity), "{}", LogSystemInternals::FormatFileLine(record));
                return;
            }

            m_Logger->log(ToSpdlogLevel(record.severity), "[{}] {}", record.GetChannelName(), record.message);
        }

        void Flush() override
        {
            if (m_Logger != nullptr)
            {
                m_Logger->flush();
            }
        }

    private:
        static spdlog::level::level_enum ToSpdlogLevel(LogSeverity severity)
        {
            switch (severity)
            {
                case LogSeverity::Trace: return spdlog::level::trace;
                case LogSeverity::Debug: return spdlog::level::debug;
                case LogSeverity::Info: return spdlog::level::info;
                case LogSeverity::Warn: return spdlog::level::warn;
                case LogSeverity::Error: return spdlog::level::err;
                case LogSeverity::Fatal: return spdlog::level::critical;
                default: return spdlog::level::info;
            }
        }

        std::shared_ptr<spdlog::logger> m_Logger;
        bool m_FullLine = false;
    };

    void LogSystem::Initialize()
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        if (state.initialized)
        {
            return;
        }

        auto stdoutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        stdoutSink->set_pattern("%^[%T] %v%$");

        auto logger = std::make_shared<spdlog::logger>("minEngine", stdoutSink);
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::err);
        spdlog::register_logger(logger);

        AddSink(std::make_shared<SpdlogTextSink>(logger, false));

        if (state.TryOpenSessionFile())
        {
            if (auto fileLogger = spdlog::get("minEngine.file"))
            {
                AddSink(std::make_shared<SpdlogTextSink>(fileLogger, true));
            }
        }

        AddSink(std::make_shared<LogConsoleSink>());
        spdlog::flush_every(std::chrono::seconds(1));

        state.initialized = true;
        ME_LOG(LogCore, Info, "LogSystem Initialized");
    }

    void LogSystem::Shutdown()
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        if (!state.initialized)
        {
            return;
        }

        ME_LOG(LogCore, Info, "LogSystem Shutdown");
        Flush();

        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.sinks.clear();
        }

        spdlog::shutdown();
        state.sessionLogPath.clear();
        state.sessionPointerPath.clear();
        state.executableStem.clear();
        state.initialized = false;
    }

    void LogSystem::SetSessionLogDirectory(const std::filesystem::path& directory)
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        if (state.initialized)
        {
            return;
        }
        state.sessionDirectoryOverride = directory;
    }

    std::filesystem::path LogSystem::GetSessionLogPath()
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        std::lock_guard<std::mutex> lock(state.mutex);
        return state.sessionLogPath;
    }

    std::filesystem::path LogSystem::GetSessionPointerPath()
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        std::lock_guard<std::mutex> lock(state.mutex);
        return state.sessionPointerPath;
    }

    bool LogSystem::PublishSessionPointer(const std::filesystem::path& projectRoot)
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        std::filesystem::path logPath;
        std::string stem;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            logPath = state.sessionLogPath;
            stem = state.executableStem;
            state.sessionPointerPath.clear();
        }

        if (logPath.empty() || projectRoot.empty())
        {
            ME_LOG(LogCore, Warn, "Session log file is not open; skipped project log pointer.");
            return false;
        }

        if (stem.empty())
        {
            stem = "process";
        }

        namespace fs = std::filesystem;
        const fs::path pointerDirectory = projectRoot / "Saved" / "Logs";
        std::error_code createError;
        fs::create_directories(pointerDirectory, createError);
        if (createError)
        {
            ME_LOG(LogCore, Warn, "Failed to create session log pointer directory: {}", pointerDirectory.string());
            return false;
        }

        const fs::path pointerPath = pointerDirectory / ("latest-session-" + stem + ".txt");
        std::ofstream output(pointerPath, std::ios::trunc);
        if (!output)
        {
            ME_LOG(LogCore, Warn, "Failed to write session log pointer: {}", pointerPath.string());
            return false;
        }
        output << logPath.string() << '\n';
        if (!output)
        {
            ME_LOG(LogCore, Warn, "Failed to write session log pointer: {}", pointerPath.string());
            return false;
        }

        std::lock_guard<std::mutex> lock(state.mutex);
        state.sessionPointerPath = pointerPath;
        return true;
    }

    std::string LogSystem::FormatLocalTimestamp(std::chrono::system_clock::time_point timePoint)
    {
        const std::time_t seconds = std::chrono::system_clock::to_time_t(timePoint);
        std::tm localTime{};
#if defined(_WIN32)
        localtime_s(&localTime, &seconds);
#else
        localtime_r(&seconds, &localTime);
#endif
        const auto millis =
            std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()) % 1000;
        int millisecond = static_cast<int>(millis.count());
        if (millisecond < 0)
        {
            millisecond = 0;
        }
        return fmt::format(
            "{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
            localTime.tm_year + 1900,
            localTime.tm_mon + 1,
            localTime.tm_mday,
            localTime.tm_hour,
            localTime.tm_min,
            localTime.tm_sec,
            millisecond);
    }

    LogSystem& LogSystem::Get()
    {
        static LogSystem instance;
        return instance;
    }

    void LogSystem::AddSink(std::shared_ptr<ILogSink> sink)
    {
        if (sink == nullptr)
        {
            return;
        }

        LogSystemInternals& state = LogSystemInternals::Get();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.sinks.push_back(std::move(sink));
    }

    void LogSystem::Emit(
        const LogChannelBase& channel,
        LogSeverity severity,
        std::string message,
        LogSourceLocation source)
    {
        if (channel.IsSuppressed(severity))
        {
            return;
        }

        LogRecord record;
        record.timestamp = std::chrono::system_clock::now();
        record.severity = severity;
        record.channel = &channel;
        record.channelName = channel.GetName();
        record.message = std::move(message);
        record.source = source;
        record.threadId = LogSystemInternals::CurrentThreadId();
        record.EnsureLocalDisplayTime();

        {
            LogSystemInternals& state = LogSystemInternals::Get();
            std::lock_guard<std::mutex> lock(state.mutex);
            for (const std::shared_ptr<ILogSink>& sink : state.sinks)
            {
                if (sink != nullptr)
                {
                    sink->Write(record);
                }
            }
        }

        if (severity == LogSeverity::Fatal)
        {
            Flush();
            std::abort();
        }
    }

    void LogSystem::SetChannelSeverity(LogChannelBase& channel, LogSeverity severity)
    {
        channel.SetSeverity(severity);
    }

    void LogSystem::SetChannelSeverityByName(std::string_view name, LogSeverity severity)
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        std::lock_guard<std::mutex> lock(state.mutex);
        for (LogChannelBase* channel : state.channels)
        {
            if (channel != nullptr && name == channel->GetName())
            {
                channel->SetSeverity(severity);
            }
        }
    }

    void LogSystem::Flush()
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        std::lock_guard<std::mutex> lock(state.mutex);
        for (const std::shared_ptr<ILogSink>& sink : state.sinks)
        {
            if (sink != nullptr)
            {
                sink->Flush();
            }
        }
    }

    void LogSystem::RegisterChannel(LogChannelBase& channel)
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        std::lock_guard<std::mutex> lock(state.mutex);
        for (LogChannelBase* existing : state.channels)
        {
            if (existing == &channel)
            {
                return;
            }
        }
        state.channels.push_back(&channel);
    }

    void LogSystem::UnregisterChannel(LogChannelBase& channel)
    {
        LogSystemInternals& state = LogSystemInternals::Get();
        std::lock_guard<std::mutex> lock(state.mutex);
        for (auto it = state.channels.begin(); it != state.channels.end(); ++it)
        {
            if (*it == &channel)
            {
                state.channels.erase(it);
                return;
            }
        }
    }

    void LogSystem::ForEachRegisteredChannel(const std::function<void(LogChannelBase&)>& fn)
    {
        if (!fn)
        {
            return;
        }

        LogSystemInternals& state = LogSystemInternals::Get();
        std::vector<LogChannelBase*> snapshot;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            snapshot = state.channels;
        }

        for (LogChannelBase* channel : snapshot)
        {
            if (channel != nullptr)
            {
                fn(*channel);
            }
        }
    }
}
