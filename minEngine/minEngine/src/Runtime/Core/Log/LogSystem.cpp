#include "LogSystem.h"

#include "LogConsole.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

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

        static uint32_t CurrentThreadId()
        {
            const auto id = std::this_thread::get_id();
            return static_cast<uint32_t>(std::hash<std::thread::id>{}(id));
        }

    private:
        LogSystemInternals() = default;
    };

    class SpdlogTextSink final : public ILogSink
    {
    public:
        explicit SpdlogTextSink(std::shared_ptr<spdlog::logger> logger)
            : m_Logger(std::move(logger))
        {
        }

        void Write(const LogRecord& record) override
        {
            if (m_Logger == nullptr)
            {
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

        AddSink(std::make_shared<SpdlogTextSink>(logger));
        AddSink(std::make_shared<LogConsoleSink>());

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
        state.initialized = false;
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
