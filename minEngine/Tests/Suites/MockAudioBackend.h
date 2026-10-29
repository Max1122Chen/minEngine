#pragma once

#include "Runtime/Function/Audio/AudioClipLease.h"
#include "Runtime/Function/Audio/Backend/IAudioBackend.h"
#include "Runtime/Resource/AudioClip.h"

#include <memory>
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

            // What a real device thread would read from, held the same way the miniaudio backend holds
            // it: the lease owns the clip, and the sample pointer is only borrowed from the lease.
            AudioClipLease Lease{};
            uint32_t PcmReadCount{0};
        };

        bool Initialize() override { return true; }
        void Shutdown() override
        {
            for (VoiceRecord& voice : m_Voices)
            {
                voice.Lease.Reset();
            }

            m_Voices.clear();
            m_CreateVoiceCount = 0;
            m_DestroyVoiceCount = 0;
        }

        void Update() override {}

        BackendVoiceHandle CreateVoice(std::shared_ptr<const AudioClip> clip) override
        {
            AudioClipLease lease(std::move(clip));
            if (!lease.IsValid())
            {
                return BackendVoiceHandle{};
            }

            ++m_CreateVoiceCount;

            for (uint32_t index = 0; index < static_cast<uint32_t>(m_Voices.size()); ++index)
            {
                if (!m_Voices[index].bAllocated)
                {
                    m_Voices[index] = VoiceRecord{};
                    m_Voices[index].bAllocated = true;
                    m_Voices[index].Lease = std::move(lease);
                    return BackendVoiceHandle{index};
                }
            }

            const uint32_t index = static_cast<uint32_t>(m_Voices.size());
            m_Voices.push_back(VoiceRecord{});
            m_Voices.back().bAllocated = true;
            m_Voices.back().Lease = std::move(lease);
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
            voice->Lease.Reset();
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

        void SetListenerEnabled(bool enabled) override { m_bListenerEnabled = enabled; }

        bool IsListenerEnabled() const override { return m_bListenerEnabled; }

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

        // A device thread reaches the end of a buffer on its own; this is the smoke suite driving
        // that edge so the natural-end path can be observed without waiting on real audio.
        void SimulatePlaybackEnd(BackendVoiceHandle handle)
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice != nullptr)
            {
                voice->bPlaying = false;
            }
        }

        // Reads the buffer the way the device thread would: through the leased pointer, at head and
        // tail so a caller cannot pass a truncated buffer off as the whole clip.
        bool ReadBorrowedSamples(BackendVoiceHandle handle, float& firstSample, float& lastSample)
        {
            VoiceRecord* voice = GetVoice(handle);
            if (voice == nullptr || !voice->Lease.IsValid())
            {
                return false;
            }

            const float* const pcm = voice->Lease.GetPcmData();
            const size_t sampleCount = voice->Lease.GetSampleCount();
            if (pcm == nullptr || sampleCount == 0)
            {
                return false;
            }

            ++voice->PcmReadCount;
            firstSample = pcm[0];
            lastSample = pcm[sampleCount - 1];
            return true;
        }

        // The clip a voice is currently reading samples from, or null when no voice borrows anything.
        const AudioClip* GetBorrowedClipAddress(BackendVoiceHandle handle) const
        {
            const VoiceRecord* voice = GetVoice(handle);
            return voice != nullptr && voice->Lease.IsValid() ? voice->Lease.GetClip() : nullptr;
        }

        // The exact sample address the voice reads from; a caller compares it against the clip it
        // believes it still owns.
        const float* GetBorrowedPcm(BackendVoiceHandle handle) const
        {
            const VoiceRecord* voice = GetVoice(handle);
            return voice != nullptr && voice->Lease.IsValid() ? voice->Lease.GetPcmData() : nullptr;
        }

        uint32_t GetBorrowedReadCount(BackendVoiceHandle handle) const
        {
            const VoiceRecord* voice = GetVoice(handle);
            return voice != nullptr ? voice->PcmReadCount : 0;
        }

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
        bool m_bListenerEnabled{false};
        uint32_t m_CreateVoiceCount{0};
        uint32_t m_DestroyVoiceCount{0};
    };
}
