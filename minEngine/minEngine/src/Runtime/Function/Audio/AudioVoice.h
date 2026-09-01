#pragma once

#include "Runtime/Function/Audio/AudioTypes.h"
#include "Runtime/Function/Audio/Backend/AudioBackendTypes.h"

namespace minEngine
{
    class IAudioBackend;
    class AudioMixer;
    class AudioClip;
    class AudioComponent;
    class Scene;

    class AudioVoice
    {
    public:
        AudioVoice() = default;

        AudioVoiceId GetId() const { return m_Id; }
        EAudioVoiceState GetState() const { return m_State; }

        bool IsPlaying() const { return m_State == EAudioVoiceState::Playing; }
        bool IsActive() const
        {
            return m_State == EAudioVoiceState::Playing || m_State == EAudioVoiceState::Paused;
        }

        bool IsSpatialized() const { return m_Spatial.bSpatialized; }

        void Configure(
            std::shared_ptr<AudioClip> clip,
            EAudioBusId bus,
            float volume,
            float pitch,
            bool loop,
            const AudioSpatialSettings& spatial,
            const Vector3& worldPosition,
            AudioComponent* ownerComponent,
            Scene* ownerScene);

        void Play(IAudioBackend& backend, const AudioMixer& mixer);
        void Stop(IAudioBackend& backend);
        void Pause(IAudioBackend& backend);
        void Resume(IAudioBackend& backend);

        void SetVolume(float volume, IAudioBackend& backend, const AudioMixer& mixer);
        void SetPitch(float pitch, IAudioBackend& backend);
        void SetLoop(bool loop, IAudioBackend& backend);

        void SetWorldPosition(const Vector3& position, IAudioBackend& backend);
        void ApplySpatialSettings(const AudioSpatialSettings& settings, IAudioBackend& backend);

        void RefreshGain(IAudioBackend& backend, const AudioMixer& mixer);
        void NotifyPlaybackFinished();

        Scene* GetOwnerScene() const { return m_OwnerScene; }
        AudioComponent* GetOwnerComponent() const { return m_OwnerComponent; }

    private:
        friend class AudioSystem;

        void Reset(IAudioBackend& backend);
        bool EnsureBackendVoice(IAudioBackend& backend);

        AudioVoiceId m_Id{InvalidAudioVoiceId};
        std::weak_ptr<AudioClip> m_Clip{};

        EAudioBusId m_Bus{EAudioBusId::SFX};
        float m_Volume{1.0f};
        float m_Pitch{1.0f};
        bool m_bLoop{false};
        EAudioVoiceState m_State{EAudioVoiceState::Stopped};

        AudioSpatialSettings m_Spatial{};
        Vector3 m_WorldPosition{};

        BackendVoiceHandle m_BackendHandle{};
        bool m_bBackendVoiceAllocated{false};

        AudioComponent* m_OwnerComponent{nullptr};
        Scene* m_OwnerScene{nullptr};
    };
}
