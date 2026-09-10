#pragma once

#include "Runtime/Function/Audio/AudioSystem.h"
#include "Runtime/Function/Audio/Backend/IAudioBackend.h"

#include <memory>
#include <utility>

namespace minEngine::Testing
{
    template<>
    class TestAccess<AudioSystem>
    {
    public:
        static void SetInstance(AudioSystem* instance)
        {
            AudioSystem::SetInstance(instance);
        }

        static void InitializeWithBackend(AudioSystem& system, std::unique_ptr<IAudioBackend> backend)
        {
            system.InitializeWithBackend(std::move(backend));
        }
    };
}
