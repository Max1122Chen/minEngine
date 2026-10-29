#pragma once

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Resource/AudioClip.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace minEngine
{
    // A device reads the samples of a clip through a pointer, and miniaudio keeps that pointer
    // instead of copying the samples out of it. Whoever hands the pointer over therefore has to keep
    // the clip alive for as long as the buffer can be read, which is what this lease is: ownership of
    // the clip, plus the address and shape of the buffer derived from it.
    //
    // GetPcmData() re-checks that the address it hands out is still the one the owning clip holds, so
    // a caller that keeps the raw pointer without keeping the lease reports a contract violation
    // instead of reading freed memory in silence.
    class AudioClipLease
    {
    public:
        AudioClipLease() = default;

        explicit AudioClipLease(std::shared_ptr<const AudioClip> clip)
            : m_Clip(std::move(clip))
        {
            if (m_Clip == nullptr || !m_Clip->IsValid())
            {
                m_Clip.reset();
                return;
            }

            const std::vector<float>& pcm = m_Clip->GetPcmData();
            m_Pcm = pcm.data();
            m_SampleCount = pcm.size();
            m_FrameCount = m_Clip->GetFrameCount();
            m_ChannelCount = m_Clip->GetFormat().ChannelCount;
            m_SampleRate = m_Clip->GetFormat().SampleRate;
        }

        bool IsValid() const
        {
            return m_Clip != nullptr && m_Pcm != nullptr && m_SampleCount > 0 && m_FrameCount > 0
                && m_ChannelCount > 0;
        }

        const AudioClip* GetClip() const { return m_Clip.get(); }

        // The only way to reach the samples. A build that traps reports a clip released under a device
        // as a crash with this message; a build that only records still never hands out a stale
        // pointer, so the device thread reads null instead of freed memory.
        const float* GetPcmData() const
        {
            const bool bOwned = IsValid() && m_Clip->GetPcmData().data() == m_Pcm;
            ME_ASSERT(
                bOwned,
                "AudioClipLease: the leased pcm buffer is not owned any more; a clip was released "
                "while a device could still read its samples");

            return bOwned ? m_Pcm : nullptr;
        }

        size_t GetSampleCount() const { return m_SampleCount; }
        uint64_t GetFrameCount() const { return m_FrameCount; }
        uint32_t GetChannelCount() const { return m_ChannelCount; }
        uint32_t GetSampleRate() const { return m_SampleRate; }

        // Drops the ownership. Only valid once nothing can read the buffer any more.
        void Reset()
        {
            m_Clip.reset();
            m_Pcm = nullptr;
            m_SampleCount = 0;
            m_FrameCount = 0;
            m_ChannelCount = 0;
            m_SampleRate = 0;
        }

    private:
        std::shared_ptr<const AudioClip> m_Clip{};
        const float* m_Pcm{nullptr};
        size_t m_SampleCount{0};
        uint64_t m_FrameCount{0};
        uint32_t m_ChannelCount{0};
        uint32_t m_SampleRate{0};
    };
}
