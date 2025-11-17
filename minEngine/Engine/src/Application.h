#pragma once

#include "Core.h"

namespace minEngine
{
    class Application
    {
    public:
        Application();
        virtual ~Application();

        void Run();
    };

    // To be defined in client
    Application* CreateApplication();
}

