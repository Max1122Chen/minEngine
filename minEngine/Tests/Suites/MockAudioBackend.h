#pragma once

#include "Runtime/Function/Audio/Backend/IAudioBackend.h"
#include "Runtime/Resource/AudioClip.h"

#include <unordered_map>
#include <vector>

namespace minEngine
{
    class MockAudioBackend final : public IAudioBackend
    {
    public:
        struct VoiceRecord
        {
            bool bAllocated{false};
            bool bPlaying{false};
            bool bLoop{false};
            float Volume{1.0f};
            float Pitch{1.0f};
            Vector3 Position{};
            AudioSpatialSettings Spatial{};
            bool bSpatializationEnabled{true};
        };

        bool Initialize() override { return true; }
        void Shutdown() override
        {
            m_Voices.clear();
            m_CreateVoiceCount = 0;
            m_DestroyVoiceCount = 0;
        }

        void Update() override {}

        BackendVoiceHandle CreateVoice(const AudioClip& clip) override
        {
            (void)clip;
            ++m_CreateVoiceCount;

            for (uint32_t index = 0; index < static_cast<uint32_t>(m_Voices.size()); ++index)
            {
                if (!m_Voices[index].bAllocated)
                {
                    m_Voices[index] = VoiceRecord{};
                    m_Voices[index].bAllocated = true;
                    return BackendVoiceHandle{index};
                }
            }

            const uint32_t index = static_cast<uint32_t>(m_Voices.size());
            m_Voices.push_back(VoiceRecord{});
            m_Voices.back().bAllocated = true;
            return BackendVoiceHandle{index};
        }

        void DestroyVoice(BackendVoiceHandle handle) override
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice == nullptr)
            {
                return;
            }

            ++m_DestroyVoiceCount;
            voice->bAllocated = false;
            voice->bPlaying = false;
        }

        bool IsVoicePlaying(BackendVoiceHandle handle) const override
        {
            const VoiceRecord* voice = GetVoice(handle);
            return voice != nullptr && voice->bPlaying;
        }

        void PlayVoice(BackendVoiceHandle handle, bool loop) override
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice == nullptr)
            {
                return;
            }

            voice->bLoop = loop;
            voice->bPlaying = true;
        }

        void StopVoice(BackendVoiceHandle handle) override
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice == nullptr)
            {
                return;
            }

            voice->bPlaying = false;
        }

        void PauseVoice(BackendVoiceHandle handle) override { StopVoice(handle); }

        void ResumeVoice(BackendVoiceHandle handle) override { PlayVoice(handle, false); }

        void SetVoiceVolume(BackendVoiceHandle handle, float linearGain) override
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice != nullptr)
            {
                voice->Volume = linearGain;
            }
        }

        void SetVoicePitch(BackendVoiceHandle handle, float pitch) override
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice != nullptr)
            {
                voice->Pitch = pitch;
            }
        }

        void SetListener(const AudioListenerState& listener) override { m_Listener = listener; }

        void SetVoiceWorldPosition(BackendVoiceHandle handle, const Vector3& worldPosition) override
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice != nullptr)
            {
                voice->Position = worldPosition;
            }
        }

        void SetVoiceSpatialSettings(BackendVoiceHandle handle, const AudioSpatialSettings& settings) override
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice != nullptr)
            {
                voice->Spatial = settings;
            }
        }

        void SetVoiceSpatializationEnabled(BackendVoiceHandle handle, bool enabled) override
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice != nullptr)
            {
                voice->bSpatializationEnabled = enabled;
            }
        }

        uint32_t GetCreateVoiceCount() const { return m_CreateVoiceCount; }
        uint32_t GetDestroyVoiceCount() const { return m_DestroyVoiceCount; }
        const AudioListenerState& GetListener() const { return m_Listener; }

        const VoiceRecord* GetVoiceRecord(BackendVoiceHandle handle) const { return GetVoice(handle); }

    private:
        VoiceRecord* GetVoice(BackendVoiceHandle handle)
        {
            if (!handle.IsValid() || handle.Index >= m_Voices.size())
            {
                return nullptr;
            }

            VoiceRecord& voice = m_Voices[handle.Index];
            return voice.bAllocated ? &voice : nullptr;
        }

        const VoiceRecord* GetVoice(BackendVoiceHandle handle) const
        {
            return const_cast<MockAudioBackend*>(this)->GetVoice(handle);
        }

        std::vector<VoiceRecord> m_Voices;
        AudioListenerState m_Listener{};
        uint32_t m_CreateVoiceCount{0};
        uint32_t m_DestroyVoiceCount{0};
    };
}
