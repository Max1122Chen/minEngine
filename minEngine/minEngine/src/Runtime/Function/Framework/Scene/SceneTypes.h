#pragma once

#include "Core.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class Scene;

    enum class ESceneType : uint8_t
    {
        None = 0,
        Editor,
        PIE,
    };

    enum class ESceneTickPolicy : uint8_t
    {
        None,
        ViewportOnly,
        Gameplay,
    };

    enum class PlayState : uint8_t
    {
        Editing,
        Playing,
        Paused,
        Stopping,
    };

    struct SceneContext
    {
        ESceneType Type = ESceneType::None;
        ESceneTickPolicy TickPolicy = ESceneTickPolicy::None;
        int32_t PIEInstanceId = -1;
        std::shared_ptr<Scene> Scene;
        std::string ContextHandle;
    };
}
