#pragma once
#include "Core.h"

#include "Runtime/Core/CLI/CommandLineResult.h"

namespace minEngine
{
    class Application
    {
    public:
        Application()  = default;
        virtual ~Application() = default;

        virtual void Initialize(int argc, char** argv) {}
        virtual void Initialize(int argc, char** argv, const CommandLineResult& commandLine)
        {
            (void)commandLine;
            Initialize(argc, argv);
        }
        virtual void Shutdown() {}
        virtual void Run() {}
    };

    // to be defined in CLIENT
    Application* CreateApplication();
}