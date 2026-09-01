#include "Runtime/Function/Audio/Components/AudioComponent.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Audio/AudioSystem.h"
#include "Runtime/Function/Audio/AudioVoice.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Resource/AudioClip.h"

#include <algorithm>

namespace minEngine
{
    AudioComponent::AudioComponent() = default;

    AudioComponent::~AudioComponent()
    {
        Stop();
        if (AudioSystem::HasInstance())
        {
            AudioSystem::Get().UnregisterEmitter(this);
        }
    }

    Scene* AudioComponent::GetOwnerScene() const
    {
        if (m_Owner == nullptr)
        {
            return nullptr;
        }

        const MEObject* outer = m_Owner->GetOuter();
        if (outer == nullptr || !outer->IsA(Scene::StaticClass()))
        {
            return nullptr;
        }

        return const_cast<Scene*>(static_cast<const Scene*>(outer));
    }

    AudioSpatialSettings AudioComponent::BuildSpatialSettings() const
    {
        AudioSpatialSettings settings;
        settings.bSpatialized = m_bSpatialized;
        settings.MinDistance = m_MinDistance;
        settings.MaxDistance = m_MaxDistance;
        return settings;
    }

    bool AudioComponent::TryConsumePlayOnAwake()
    {
        if (!m_bPlayOnAwake || m_bPlayOnAwakeTriggered)
        {
            return false;
        }

        if (m_Clip == nullptr || !m_Clip->IsValid())
        {
            return false;
        }

        m_bPlayOnAwakeTriggered = true;
        return true;
    }

    void AudioComponent::SetOwner(GameObject* inOwner)
    {
        if (AudioSystem::HasInstance())
        {
            AudioSystem::Get().UnregisterEmitter(this);
        }

        SceneComponent::SetOwner(inOwner);

        if (AudioSystem::HasInstance() && inOwner != nullptr)
        {
            AudioSystem::Get().RegisterEmitter(this);
        }
    }

    void AudioComponent::Play()
    {
        if (!AudioSystem::HasInstance())
        {
            ME_CORE_WARN("AudioComponent::Play: AudioSystem is not available.");
            return;
        }

        if (m_Clip == nullptr || !m_Clip->IsValid())
        {
            ME_CORE_WARN("AudioComponent::Play: clip is invalid.");
            return;
        }

        if (m_ActiveVoice.IsValid())
        {
            AudioSystem::Get().StopVoice(m_ActiveVoice);
            m_ActiveVoice = {};
        }

        AudioPlayParams params;
        params.Clip = m_Clip;
        params.Bus = m_Bus;
        params.Volume = m_Volume;
        params.Pitch = m_Pitch;
        params.bLoop = m_bLoop;
        params.Spatial = BuildSpatialSettings();
        params.WorldPosition = GetWorldPosition();
        params.OwnerComponent = this;
        params.OwnerScene = GetOwnerScene();

        AudioPlayResult result = AudioSystem::Get().Play(params);
        if (!result.bSuccess)
        {
            ME_CORE_WARN("AudioComponent::Play failed: {}", result.ErrorMessage);
            return;
        }

        m_ActiveVoice = result.Voice;
    }

    void AudioComponent::Stop()
    {
        if (!m_ActiveVoice.IsValid() || !AudioSystem::HasInstance())
        {
            m_ActiveVoice = {};
            return;
        }

        AudioSystem::Get().StopVoice(m_ActiveVoice);
        m_ActiveVoice = {};
    }

    bool AudioComponent::IsPlaying() const
    {
        if (!m_ActiveVoice.IsValid() || !AudioSystem::HasInstance())
        {
            return false;
        }

        const AudioVoice* voice = AudioSystem::Get().FindVoice(m_ActiveVoice);
        return voice != nullptr && voice->IsPlaying();
    }

    void AudioComponent::SetVolume(float volume)
    {
        m_Volume = std::clamp(volume, kMinAudioVolume, kMaxAudioVolume);
        if (m_ActiveVoice.IsValid() && AudioSystem::HasInstance())
        {
            if (AudioVoice* voice = AudioSystem::Get().FindVoice(m_ActiveVoice))
            {
                voice->SetVolume(m_Volume, *AudioSystem::Get().GetBackend(), AudioSystem::Get().GetMixer());
            }
        }
    }

    void AudioComponent::SetPitch(float pitch)
    {
        m_Pitch = std::clamp(pitch, kMinAudioPitch, kMaxAudioPitch);
        if (m_ActiveVoice.IsValid() && AudioSystem::HasInstance())
        {
            if (AudioVoice* voice = AudioSystem::Get().FindVoice(m_ActiveVoice))
            {
                voice->SetPitch(m_Pitch, *AudioSystem::Get().GetBackend());
            }
        }
    }

    void AudioComponent::SetLooping(bool loop)
    {
        m_bLoop = loop;
        if (m_ActiveVoice.IsValid() && AudioSystem::HasInstance())
        {
            if (AudioVoice* voice = AudioSystem::Get().FindVoice(m_ActiveVoice))
            {
                voice->SetLoop(loop, *AudioSystem::Get().GetBackend());
            }
        }
    }

    void AudioComponent::SetSpatialized(bool spatialized)
    {
        m_bSpatialized = spatialized;
    }

    void AudioComponent::SetMinDistance(float distance)
    {
        m_MinDistance = std::max(distance, 0.0f);
    }

    void AudioComponent::SetMaxDistance(float distance)
    {
        m_MaxDistance = std::max(distance, m_MinDistance);
    }
}
