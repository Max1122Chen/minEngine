#pragma once

#include "Runtime/Function/Audio/AudioMixer.h"
#include "Runtime/Function/Audio/AudioTypes.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class IAudioBackend;
    class AudioVoice;
    class AudioComponent;
    class AudioListenerComponent;
    class AudioClip;
    class Scene;
    class SceneComponent;

    class AudioSystem
    {
    public:
        AudioSystem();
        ~AudioSystem();

        void Initialize();
        void Shutdown();

        static bool HasInstance();
        static AudioSystem& Get();

        void Tick(float deltaTime);

        IAudioBackend* GetBackend() const { return m_Backend.get(); }
        AudioMixer& GetMixer() { return m_Mixer; }
        const AudioMixer& GetMixer() const { return m_Mixer; }

        AudioPlayResult Play(const AudioPlayParams& params);
        AudioPlayResult Play2D(
            std::shared_ptr<AudioClip> clip,
            EAudioBusId bus = EAudioBusId::SFX,
            float volume = 1.0f);
        AudioPlayResult Play3D(
            std::shared_ptr<AudioClip> clip,
            const Vector3& worldPosition,
            const AudioSpatialSettings& spatial = {},
            EAudioBusId bus = EAudioBusId::SFX,
            float volume = 1.0f);

        bool StopVoice(AudioVoiceHandle handle);
        bool PauseVoice(AudioVoiceHandle handle);
        bool ResumeVoice(AudioVoiceHandle handle);
        void StopAllVoices();

        void RegisterEmitter(AudioComponent* component);
        void UnregisterEmitter(AudioComponent* component);

        void RegisterListener(AudioListenerComponent* listener);
        void UnregisterListener(AudioListenerComponent* listener);

        void OnSceneUnloaded(Scene* scene);

        AudioVoice* FindVoice(AudioVoiceHandle handle);
        const AudioVoice* FindVoice(AudioVoiceHandle handle) const;
        uint32_t GetActiveVoiceCount() const;

    private:
        friend class Engine;
        friend class AudioSmokeTestScope;

        static void SetInstance(AudioSystem* instance);
        void InitializeWithBackend(std::unique_ptr<IAudioBackend> backend);

        AudioVoice* AllocateVoice();
        void StopAndFreeVoice(AudioVoice* voice);

        void SyncListenerToBackend();
        void SyncEmittersToBackend();
        void PushActiveListenerToBackend();
        void ProcessPlayOnAwake();
        void UpdateVoiceStates();
        void ValidateSpatializedSources();
        void WarnMissingListenerForSpatializedAudio();
        void LogSpatialAudioDiagnostics(bool forceLog);
        void LogSceneComponentTransformDiagnostics(const char* role, const SceneComponent* component);

        static AudioSystem* s_Instance;

        bool m_Initialized{false};
        std::unique_ptr<IAudioBackend> m_Backend;
        AudioMixer m_Mixer;

        std::vector<std::unique_ptr<AudioVoice>> m_VoiceSlots;
        std::unordered_map<AudioVoiceId, AudioVoice*> m_VoiceById;
        AudioVoiceId m_NextVoiceId{1};

        std::vector<AudioComponent*> m_Emitters;
        AudioListenerComponent* m_ActiveListener{nullptr};

        AudioListenerState m_CachedListener{};
        bool m_bListenerDirty{true};
        bool m_bWarnedMissingListenerForSpatial{false};
        float m_SpatialDiagnosticsLogTimer{0.0f};
    };
}
