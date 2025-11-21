#pragma once
#include "Core.h"

namespace minEngine
{
    class LogSystem;
    class RenderSystem;


    class RuntimeGlobalContext
    {
    public:
        RuntimeGlobalContext() = default;
        ~RuntimeGlobalContext() = default;

        static RuntimeGlobalContext& GetInstance() { return *s_Instance; }


        void StartSystems();
        void ShutdownSystems();

    private:
        // Singleton instance
        static std::shared_ptr<RuntimeGlobalContext> s_Instance;


    private:
        // Add global systems and contexts here
        std::shared_ptr<LogSystem> m_LogSystem;
        std::shared_ptr<RenderSystem> m_RenderSystem;
    };
}