#include "Runtime/Function/Audio/AudioSystem.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Audio/AudioLimits.h"
#include "Runtime/Function/Audio/AudioVoice.h"
#include "Runtime/Function/Audio/Backend/IAudioBackend.h"
#include "Runtime/Function/Audio/Backend/MiniaudioBackend.h"
#include "Runtime/Function/Audio/Components/AudioComponent.h"
#include "Runtime/Function/Audio/Components/AudioListenerComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Resource/AudioClip.h"

#include <algorithm>

namespace minEngine
{
    AudioSystem* AudioSystem::s_Instance = nullptr;

    AudioSystem::AudioSystem() = default;

    AudioSystem::~AudioSystem()
    {
        Shutdown();
    }

    void AudioSystem::SetInstance(AudioSystem* instance)
    {
        s_Instance = instance;
    }

    AudioSystem& AudioSystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "AudioSystem is not initialized");
        return *s_Instance;
    }

    bool AudioSystem::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void AudioSystem::Initialize()
    {
        InitializeWithBackend(std::make_unique<MiniaudioBackend>());
    }

    void AudioSystem::InitializeWithBackend(std::unique_ptr<IAudioBackend> backend)
    {
        if (m_Initialized)
        {
            return;
        }

        m_Backend = std::move(backend);
        if (m_Backend == nullptr || !m_Backend->Initialize())
        {
            ME_CORE_ERROR("AudioSystem: backend initialization failed.");
            m_Backend.reset();
            return;
        }

        m_VoiceSlots.reserve(kMaxAudioVoices);
        m_Initialized = true;
    }

    void AudioSystem::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        StopAllVoices();
        m_Emitters.clear();
        m_ActiveListener = nullptr;

        if (m_Backend)
        {
            m_Backend->Shutdown();
            m_Backend.reset();
        }

        m_VoiceSlots.clear();
        m_VoiceById.clear();
        m_NextVoiceId = 1;
        m_Initialized = false;
    }

    void AudioSystem::Tick(float deltaTime)
    {
        (void)deltaTime;
        if (!m_Initialized || !m_Backend)
        {
            return;
        }

        UpdateVoiceStates();
        SyncListenerToBackend();
        SyncEmittersToBackend();
        m_Backend->Update();
    }

    AudioPlayResult AudioSystem::Play(const AudioPlayParams& params)
    {
        AudioPlayResult result;
        if (!m_Initialized || !m_Backend)
        {
            result.ErrorMessage = "AudioSystem is not initialized";
            return result;
        }

        if (params.Clip == nullptr || !params.Clip->IsValid())
        {
            result.ErrorMessage = "invalid audio clip";
            return result;
        }

        AudioVoice* voice = AllocateVoice();
        if (voice == nullptr)
        {
            result.ErrorMessage = "voice pool exhausted";
            return result;
        }

        voice->Configure(
            params.Clip,
            params.Bus,
            params.Volume,
            params.Pitch,
            params.bLoop,
            params.Spatial,
            params.WorldPosition,
            params.OwnerComponent,
            params.OwnerScene);
        voice->Play(*m_Backend, m_Mixer);

        result.Voice = AudioVoiceHandle{voice->GetId()};
        result.bSuccess = true;
        return result;
    }

    AudioPlayResult AudioSystem::Play2D(
        std::shared_ptr<AudioClip> clip,
        EAudioBusId bus,
        float volume)
    {
        AudioPlayParams params;
        params.Clip = std::move(clip);
        params.Bus = bus;
        params.Volume = volume;
        params.Spatial.bSpatialized = false;
        return Play(params);
    }

    AudioPlayResult AudioSystem::Play3D(
        std::shared_ptr<AudioClip> clip,
        const Vector3& worldPosition,
        const AudioSpatialSettings& spatial,
        EAudioBusId bus,
        float volume)
    {
        AudioPlayParams params;
        params.Clip = std::move(clip);
        params.Bus = bus;
        params.Volume = volume;
        params.Spatial = spatial;
        params.WorldPosition = worldPosition;
        return Play(params);
    }

    bool AudioSystem::StopVoice(AudioVoiceHandle handle)
    {
        AudioVoice* voice = FindVoice(handle);
        if (voice == nullptr || !m_Backend)
        {
            return false;
        }

        StopAndFreeVoice(voice);
        return true;
    }

    bool AudioSystem::PauseVoice(AudioVoiceHandle handle)
    {
        AudioVoice* voice = FindVoice(handle);
        if (voice == nullptr || !m_Backend)
        {
            return false;
        }

        voice->Pause(*m_Backend);
        return true;
    }

    bool AudioSystem::ResumeVoice(AudioVoiceHandle handle)
    {
        AudioVoice* voice = FindVoice(handle);
        if (voice == nullptr || !m_Backend)
        {
            return false;
        }

        voice->Resume(*m_Backend);
        return true;
    }

    void AudioSystem::StopAllVoices()
    {
        if (!m_Backend)
        {
            m_VoiceSlots.clear();
            m_VoiceById.clear();
            return;
        }

        for (auto& voiceSlot : m_VoiceSlots)
        {
            if (voiceSlot)
            {
                voiceSlot->Reset(*m_Backend);
            }
        }

        m_VoiceSlots.clear();
        m_VoiceById.clear();
    }

    void AudioSystem::RegisterEmitter(AudioComponent* component)
    {
        if (component == nullptr)
        {
            return;
        }

        const auto existing = std::find(m_Emitters.begin(), m_Emitters.end(), component);
        if (existing == m_Emitters.end())
        {
            m_Emitters.push_back(component);
        }
    }

    void AudioSystem::UnregisterEmitter(AudioComponent* component)
    {
        if (component == nullptr)
        {
            return;
        }

        const auto iter = std::remove(m_Emitters.begin(), m_Emitters.end(), component);
        if (iter != m_Emitters.end())
        {
            m_Emitters.erase(iter, m_Emitters.end());
        }
    }

    void AudioSystem::RegisterListener(AudioListenerComponent* listener)
    {
        if (listener == nullptr)
        {
            return;
        }

        if (m_ActiveListener != nullptr && m_ActiveListener != listener)
        {
            ME_CORE_WARN("AudioSystem: replacing active listener (last registered wins).");
        }

        m_ActiveListener = listener;
        m_bListenerDirty = true;
    }

    void AudioSystem::UnregisterListener(AudioListenerComponent* listener)
    {
        if (m_ActiveListener == listener)
        {
            m_ActiveListener = nullptr;
            m_bListenerDirty = true;
        }
    }

    void AudioSystem::OnSceneUnloaded(Scene* scene)
    {
        if (scene == nullptr || !m_Backend)
        {
            return;
        }

        for (auto voiceIter = m_VoiceById.begin(); voiceIter != m_VoiceById.end();)
        {
            AudioVoice* voice = voiceIter->second;
            if (voice != nullptr && voice->GetOwnerScene() == scene)
            {
                StopAndFreeVoice(voice);
                voiceIter = m_VoiceById.begin();
                continue;
            }

            ++voiceIter;
        }

        const auto removeEmitter = [scene](AudioComponent* component) {
            if (component == nullptr || component->GetOwner() == nullptr)
            {
                return false;
            }

            const MEObject* outer = component->GetOwner()->GetOuter();
            return outer != nullptr && outer == scene;
        };

        m_Emitters.erase(
            std::remove_if(m_Emitters.begin(), m_Emitters.end(), removeEmitter),
            m_Emitters.end());

        if (m_ActiveListener != nullptr)
        {
            const GameObject* owner = m_ActiveListener->GetOwner();
            if (owner != nullptr)
            {
                const MEObject* outer = owner->GetOuter();
                if (outer == scene)
                {
                    m_ActiveListener = nullptr;
                    m_bListenerDirty = true;
                }
            }
        }
    }

    AudioVoice* AudioSystem::FindVoice(AudioVoiceHandle handle)
    {
        const auto iter = m_VoiceById.find(handle.Id);
        return iter != m_VoiceById.end() ? iter->second : nullptr;
    }

    const AudioVoice* AudioSystem::FindVoice(AudioVoiceHandle handle) const
    {
        const auto iter = m_VoiceById.find(handle.Id);
        return iter != m_VoiceById.end() ? iter->second : nullptr;
    }

    uint32_t AudioSystem::GetActiveVoiceCount() const
    {
        uint32_t count = 0;
        for (const auto& [voiceId, voice] : m_VoiceById)
        {
            (void)voiceId;
            if (voice != nullptr && voice->IsActive())
            {
                ++count;
            }
        }

        return count;
    }

    AudioVoice* AudioSystem::AllocateVoice()
    {
        if (m_VoiceById.size() >= kMaxAudioVoices)
        {
            return nullptr;
        }

        auto voice = std::make_unique<AudioVoice>();
        voice->m_Id = m_NextVoiceId++;
        AudioVoice* voicePtr = voice.get();
        m_VoiceSlots.push_back(std::move(voice));
        m_VoiceById[voicePtr->GetId()] = voicePtr;
        return voicePtr;
    }

    void AudioSystem::StopAndFreeVoice(AudioVoice* voice)
    {
        if (voice == nullptr || !m_Backend)
        {
            return;
        }

        const AudioVoiceId voiceId = voice->GetId();
        voice->Stop(*m_Backend);
        m_VoiceById.erase(voiceId);

        const auto slotIter = std::find_if(
            m_VoiceSlots.begin(),
            m_VoiceSlots.end(),
            [voice](const std::unique_ptr<AudioVoice>& slot) { return slot.get() == voice; });
        if (slotIter != m_VoiceSlots.end())
        {
            m_VoiceSlots.erase(slotIter);
        }
    }

    void AudioSystem::SyncListenerToBackend()
    {
        if (!m_Backend)
        {
            return;
        }

        if (m_ActiveListener == nullptr)
        {
            return;
        }

        AudioListenerState listenerState;
        listenerState.Position = m_ActiveListener->GetPosition();
        listenerState.Forward = m_ActiveListener->GetForwardVector();
        listenerState.Up = m_ActiveListener->GetUpVector();

        if (!m_bListenerDirty && listenerState.Position == m_CachedListener.Position
            && listenerState.Forward == m_CachedListener.Forward && listenerState.Up == m_CachedListener.Up)
        {
            return;
        }

        m_CachedListener = listenerState;
        m_bListenerDirty = false;
        m_Backend->SetListener(listenerState);
    }

    void AudioSystem::SyncEmittersToBackend()
    {
        if (!m_Backend)
        {
            return;
        }

        for (AudioComponent* emitter : m_Emitters)
        {
            if (emitter == nullptr)
            {
                continue;
            }

            const AudioVoiceHandle activeVoice = emitter->GetActiveVoiceHandle();
            AudioVoice* voice = FindVoice(activeVoice);
            if (voice == nullptr || !voice->IsActive())
            {
                continue;
            }

            voice->SetWorldPosition(emitter->GetPosition(), *m_Backend);
        }
    }

    void AudioSystem::UpdateVoiceStates()
    {
        if (!m_Backend)
        {
            return;
        }

        std::vector<AudioVoice*> finishedVoices;
        for (auto& [voiceId, voice] : m_VoiceById)
        {
            (void)voiceId;
            if (voice == nullptr || !voice->IsPlaying())
            {
                continue;
            }

            if (!voice->m_BackendHandle.IsValid())
            {
                continue;
            }

            if (!m_Backend->IsVoicePlaying(voice->m_BackendHandle))
            {
                finishedVoices.push_back(voice);
            }
        }

        for (AudioVoice* voice : finishedVoices)
        {
            StopAndFreeVoice(voice);
        }
    }
}
