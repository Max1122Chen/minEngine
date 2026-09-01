#pragma once

#include "Core.h"

namespace minEngine
{
    class Scene;
}

namespace minEngine::Command
{
    struct CommandContext
    {
        Scene* ActiveScene = nullptr;
    };
}
