#include "Runtime/Function/Audio/AudioVoice.h"

#include "Runtime/Function/Audio/AudioMixer.h"
#include "Runtime/Function/Audio/Backend/IAudioBackend.h"
#include "Runtime/Resource/AudioClip.h"

#include <algorithm>

namespace minEngine
{
    void AudioVoice::Configure(
        std::shared_ptr<AudioClip> clip,
        EAudioBusId bus,
        float volume,
        float pitch,
        bool loop,
        const AudioSpatialSettings& spatial,
        const Vector3& worldPosition,
        AudioComponent* ownerComponent,
        Scene* ownerScene)
    {
        m_Clip = clip;
        m_Bus = bus;
        m_Volume = std::clamp(volume, kMinAudioVolume, kMaxAudioVolume);
        m_Pitch = std::clamp(pitch, kMinAudioPitch, kMaxAudioPitch);
        m_bLoop = loop;
        m_Spatial = spatial;
        m_WorldPosition = worldPosition;
        m_OwnerComponent = ownerComponent;
        m_OwnerScene = ownerScene;
    }

    bool AudioVoice::EnsureBackendVoice(IAudioBackend& backend)
    {
        if (m_bBackendVoiceAllocated && m_BackendHandle.IsValid())
        {
            return true;
        }

        const std::shared_ptr<AudioClip> clip = m_Clip.lock();
        if (!clip || !clip->IsValid())
        {
            return false;
        }

        m_BackendHandle = backend.CreateVoice(*clip);
        if (!m_BackendHandle.IsValid())
        {
            return false;
        }

        m_bBackendVoiceAllocated = true;
        ApplySpatialSettings(m_Spatial, backend);
        SetWorldPosition(m_WorldPosition, backend);
        backend.SetVoicePitch(m_BackendHandle, m_Pitch);
        return true;
    }

    void AudioVoice::Play(IAudioBackend& backend, const AudioMixer& mixer)
    {
        if (!EnsureBackendVoice(backend))
        {
            return;
        }

        backend.PlayVoice(m_BackendHandle, m_bLoop);
        m_State = EAudioVoiceState::Playing;
        RefreshGain(backend, mixer);
    }

    void AudioVoice::Stop(IAudioBackend& backend)
    {
        if (m_bBackendVoiceAllocated && m_BackendHandle.IsValid())
        {
            backend.StopVoice(m_BackendHandle);
            backend.DestroyVoice(m_BackendHandle);
        }

        m_BackendHandle = {};
        m_bBackendVoiceAllocated = false;
        m_State = EAudioVoiceState::Stopped;
        m_OwnerComponent = nullptr;
        m_OwnerScene = nullptr;
        m_Clip.reset();
    }

    void AudioVoice::Pause(IAudioBackend& backend)
    {
        if (!IsPlaying() || !m_BackendHandle.IsValid())
        {
            return;
        }

        backend.PauseVoice(m_BackendHandle);
        m_State = EAudioVoiceState::Paused;
    }

    void AudioVoice::Resume(IAudioBackend& backend)
    {
        if (m_State != EAudioVoiceState::Paused || !m_BackendHandle.IsValid())
        {
            return;
        }

        backend.ResumeVoice(m_BackendHandle);
        m_State = EAudioVoiceState::Playing;
    }

    void AudioVoice::SetVolume(float volume, IAudioBackend& backend, const AudioMixer& mixer)
    {
        m_Volume = std::clamp(volume, kMinAudioVolume, kMaxAudioVolume);
        RefreshGain(backend, mixer);
    }

    void AudioVoice::SetPitch(float pitch, IAudioBackend& backend)
    {
        m_Pitch = std::clamp(pitch, kMinAudioPitch, kMaxAudioPitch);
        if (m_BackendHandle.IsValid())
        {
            backend.SetVoicePitch(m_BackendHandle, m_Pitch);
        }
    }

    void AudioVoice::SetLoop(bool loop, IAudioBackend& backend)
    {
        m_bLoop = loop;
        if (m_BackendHandle.IsValid() && IsActive())
        {
            backend.PlayVoice(m_BackendHandle, m_bLoop);
        }
    }

    void AudioVoice::SetWorldPosition(const Vector3& position, IAudioBackend& backend)
    {
        m_WorldPosition = position;
        if (m_BackendHandle.IsValid())
        {
            backend.SetVoiceWorldPosition(m_BackendHandle, m_WorldPosition);
        }
    }

    void AudioVoice::ApplySpatialSettings(const AudioSpatialSettings& settings, IAudioBackend& backend)
    {
        m_Spatial = settings;
        if (!m_BackendHandle.IsValid())
        {
            return;
        }

        backend.SetVoiceSpatialSettings(m_BackendHandle, m_Spatial);
        backend.SetVoiceSpatializationEnabled(m_BackendHandle, m_Spatial.bSpatialized);
    }

    void AudioVoice::RefreshGain(IAudioBackend& backend, const AudioMixer& mixer)
    {
        if (!m_BackendHandle.IsValid())
        {
            return;
        }

        const float effectiveGain = mixer.ComputeEffectiveGain(m_Bus, m_Volume);
        backend.SetVoiceVolume(m_BackendHandle, effectiveGain);
    }

    void AudioVoice::NotifyPlaybackFinished()
    {
        m_State = EAudioVoiceState::Stopped;
        m_BackendHandle = {};
        m_bBackendVoiceAllocated = false;
        m_OwnerComponent = nullptr;
        m_OwnerScene = nullptr;
        m_Clip.reset();
    }

    void AudioVoice::Reset(IAudioBackend& backend)
    {
        Stop(backend);
        m_Id = InvalidAudioVoiceId;
    }
}
