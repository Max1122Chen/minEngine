#pragma once
#include "Core.h"

namespace minEngine
{
    class Application
    {
    public:
        Application()  = default;
        virtual ~Application() = default;

        virtual void Initialize(int argc, char** argv) {}
        virtual void Shutdown() {}
        virtual void Run() {}
    };

    // to be defined in CLIENT
    Application* CreateApplication();
}