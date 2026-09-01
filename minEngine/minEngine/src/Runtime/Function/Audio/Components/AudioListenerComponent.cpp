#include "Runtime/Function/Audio/Components/AudioListenerComponent.h"

#include "Runtime/Function/Audio/AudioSystem.h"

namespace minEngine
{
    AudioListenerComponent::AudioListenerComponent() = default;

    AudioListenerComponent::~AudioListenerComponent()
    {
        if (AudioSystem::HasInstance())
        {
            AudioSystem::Get().UnregisterListener(this);
        }
    }

    void AudioListenerComponent::SetOwner(GameObject* inOwner)
    {
        if (AudioSystem::HasInstance())
        {
            AudioSystem::Get().UnregisterListener(this);
        }

        SceneComponent::SetOwner(inOwner);

        if (AudioSystem::HasInstance() && inOwner != nullptr)
        {
            AudioSystem::Get().RegisterListener(this);
        }
    }
}
