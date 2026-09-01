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

    void AudioListenerComponent::ApplyActivationToSystems()
    {
        if (AudioSystem::HasInstance() && m_Owner != nullptr)
        {
            AudioSystem::Get().RegisterListener(this);
        }
    }

    void AudioListenerComponent::RemoveActivationFromSystems()
    {
        if (AudioSystem::HasInstance())
        {
            AudioSystem::Get().UnregisterListener(this);
        }
    }
}
