#include "LogSystem.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace minEngine
{
    std::shared_ptr<spdlog::logger> LogSystem::s_CoreLogger;
    std::shared_ptr<spdlog::logger> LogSystem::s_ClientLogger;

    void LogSystem::Initialize()
    {
        spdlog::set_pattern("%^[%T] %n: %v%$");
        s_CoreLogger = spdlog::stdout_color_mt("MINENGINE");
        s_CoreLogger->set_level(spdlog::level::trace);

        s_ClientLogger = spdlog::stdout_color_mt("APP");
        s_ClientLogger->set_level(spdlog::level::trace);

        ME_CORE_INFO("LogSystem Initialized");
    }
}
