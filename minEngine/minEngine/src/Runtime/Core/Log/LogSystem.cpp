#include "LogSystem.h"
#include "LogConsole.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace minEngine
{
    std::shared_ptr<spdlog::logger> LogSystem::s_CoreLogger;
    std::shared_ptr<spdlog::logger> LogSystem::s_ClientLogger;

    void LogSystem::Initialize()
    {
        auto stdoutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        stdoutSink->set_pattern("%^[%T] %n: %v%$");

        auto consoleSink = std::make_shared<LogConsoleSink>();

        std::vector<spdlog::sink_ptr> sinks {stdoutSink, consoleSink};

        s_CoreLogger = std::make_shared<spdlog::logger>(LogChannelNames::Core, sinks.begin(), sinks.end());
        s_CoreLogger->set_level(spdlog::level::trace);
        spdlog::register_logger(s_CoreLogger);

        s_ClientLogger = std::make_shared<spdlog::logger>(LogChannelNames::Client, sinks.begin(), sinks.end());
        s_ClientLogger->set_level(spdlog::level::trace);
        spdlog::register_logger(s_ClientLogger);

        ME_CORE_INFO("LogSystem Initialized");
    }

    void LogSystem::Shutdown()
    {
        ME_CORE_INFO("LogSystem Shutdown");
        spdlog::shutdown();
    }

    LogSystem &LogSystem::Get()
    {
        static LogSystem instance;
        return instance;
    }
}
