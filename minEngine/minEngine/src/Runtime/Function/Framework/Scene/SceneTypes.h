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

    /** Reason passed to GameObject/Component EndPlay (CORE-F31). */
    enum class EEndPlayReason : uint8_t
    {
        Stopped = 0,
        Destroyed,
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
