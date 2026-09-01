#pragma once

#include "Runtime/Function/Audio/AudioLimits.h"
#include "Runtime/Function/Audio/AudioTypes.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    class AudioClip;
    class Scene;

    ME_CLASS(ScriptType)
    class AudioComponent : public SceneComponent
    {
        ME_GENERATED_BODY(AudioComponent)

    public:
        AudioComponent();
        ~AudioComponent() override;

        ME_FUNCTION(ScriptCallable)
        void Play();

        ME_FUNCTION(ScriptCallable)
        void Stop();

        bool IsPlaying() const;

        std::shared_ptr<AudioClip> GetClip() const { return m_Clip; }
        void SetClip(const std::shared_ptr<AudioClip>& clip) { m_Clip = clip; }

        float GetVolume() const { return m_Volume; }
        void SetVolume(float volume);

        float GetPitch() const { return m_Pitch; }
        void SetPitch(float pitch);

        bool GetLooping() const { return m_bLoop; }
        void SetLooping(bool loop);

        bool GetSpatialized() const { return m_bSpatialized; }
        void SetSpatialized(bool spatialized);

        float GetMinDistance() const { return m_MinDistance; }
        void SetMinDistance(float distance);

        float GetMaxDistance() const { return m_MaxDistance; }
        void SetMaxDistance(float distance);

        EAudioBusId GetBus() const { return m_Bus; }
        void SetBus(EAudioBusId bus) { m_Bus = bus; }

        AudioVoiceHandle GetActiveVoiceHandle() const { return m_ActiveVoice; }

    private:
        friend class AudioSystem;

        void ApplyActivationToSystems() override;
        void RemoveActivationFromSystems() override;

        bool TryConsumePlayOnAwake();

        Scene* GetOwnerScene() const;
        AudioSpatialSettings BuildSpatialSettings() const;

        ME_PROPERTY(EditAnywhere)
        std::shared_ptr<AudioClip> m_Clip;

        ME_PROPERTY(EditAnywhere)
        float m_Volume{1.0f};

        ME_PROPERTY(EditAnywhere)
        float m_Pitch{1.0f};

        ME_PROPERTY(EditAnywhere)
        bool m_bLoop{false};

        ME_PROPERTY(EditAnywhere)
        bool m_bPlayOnAwake{false};

        ME_PROPERTY(EditAnywhere)
        bool m_bSpatialized{true};

        ME_PROPERTY(EditAnywhere)
        float m_MinDistance{kDefaultMinDistance};

        ME_PROPERTY(EditAnywhere)
        float m_MaxDistance{kDefaultMaxDistance};

        ME_PROPERTY(EditAnywhere)
        EAudioBusId m_Bus{EAudioBusId::SFX};

        AudioVoiceHandle m_ActiveVoice{};
        bool m_bPlayOnAwakeTriggered{false};
    };
}

#include "Generated/Reflection/AudioComponent.gen.h"
