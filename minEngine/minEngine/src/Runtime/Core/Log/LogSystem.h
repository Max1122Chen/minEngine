#pragma once
#include "Core.h"
#include "spdlog/spdlog.h"


namespace minEngine
{
    class LogSystem
    {
    public:
        static void Initialize();

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
        
    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}

// Core log macros
#define ME_CORE_TRACE(...)    ::minEngine::LogSystem::GetCoreLogger()->trace(__VA_ARGS__)
#define ME_CORE_INFO(...)     ::minEngine::LogSystem::GetCoreLogger()->info(__VA_ARGS__)
#define ME_CORE_WARN(...)     ::minEngine::LogSystem::GetCoreLogger()->warn(__VA_ARGS__)
#define ME_CORE_ERROR(...)    ::minEngine::LogSystem::GetCoreLogger()->error(__VA_ARGS__)
#define ME_CORE_CRITICAL(...) ::minEngine::LogSystem::GetCoreLogger()->critical(__VA_ARGS__)
// Client log macros
#define ME_TRACE(...)         ::minEngine::LogSystem::GetClientLogger()->trace(__VA_ARGS__)
#define ME_INFO(...)          ::minEngine::LogSystem::GetClientLogger()->info(__VA_ARGS__)
#define ME_WARN(...)          ::minEngine::LogSystem::GetClientLogger()->warn(__VA_ARGS__)
#define ME_ERROR(...)         ::minEngine::LogSystem::GetClientLogger()->error(__VA_ARGS__)
#define ME_CRITICAL(...)      ::minEngine::LogSystem::GetClientLogger()->critical(__VA_ARGS__)

