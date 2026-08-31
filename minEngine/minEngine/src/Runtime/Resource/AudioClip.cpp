#include "Runtime/Resource/AudioClip.h"

#include "Runtime/Core/Object/ObjectManager.h"

namespace minEngine
{
    float AudioClip::GetDurationSeconds() const
    {
        if (m_Format.SampleRate == 0 || m_FrameCount == 0)
        {
            return 0.0f;
        }

        return static_cast<float>(m_FrameCount) / static_cast<float>(m_Format.SampleRate);
    }

    std::shared_ptr<AudioClip> AudioClip::CreateFromPcm(
        std::vector<float> pcmInterleaved,
        uint32_t sampleRate,
        uint16_t channelCount,
        const std::string& sourcePath)
    {
        if (pcmInterleaved.empty() || sampleRate == 0 || channelCount == 0)
        {
            return nullptr;
        }

        if (pcmInterleaved.size() % channelCount != 0)
        {
            return nullptr;
        }

        std::shared_ptr<AudioClip> clip = NewObject<AudioClip>("GeneratedAudioClip", nullptr);
        clip->m_PcmInterleaved = std::move(pcmInterleaved);
        clip->m_Format.SampleRate = sampleRate;
        clip->m_Format.ChannelCount = channelCount;
        clip->m_Format.BitsPerSample = 32;
        clip->m_Format.FrameCount = clip->m_PcmInterleaved.size() / channelCount;
        clip->m_FrameCount = clip->m_Format.FrameCount;
        clip->m_SourcePath = sourcePath;
        return clip;
    }
}
