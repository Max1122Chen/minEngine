#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

namespace minEngine
{
    class Scene;
    class PlayObjectMapping;

    class IPlayModeService
    {
    public:
        virtual ~IPlayModeService() = default;

        virtual PlayState GetPlayState() const = 0;
        virtual bool IsPlaying() const = 0;
        virtual bool EnterPlay() = 0;
        virtual void Stop() = 0;
        virtual void TickPIE(float deltaTime) = 0;

        virtual Scene* GetEditorScene() const = 0;
        virtual Scene* GetPIEScene() const = 0;
        virtual PlayObjectMapping& GetObjectMapping() = 0;
    };
}
