#pragma once
#include "Core.h"

namespace minEngine
{
    class Application
    {
    public:
        Application()  = default;
        virtual ~Application() = default;

        virtual void Initialize() {}
        virtual void Shutdown() {}
        virtual void Run() {}
    };

    // to be defined in CLIENT
    Application* CreateApplication();
}