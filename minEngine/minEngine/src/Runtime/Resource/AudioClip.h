#pragma once

#include "Core.h"
#include "Runtime/Resource/Asset.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    class AudioClipLoader;

    struct AudioClipFormat
    {
        uint32_t SampleRate{0};
        uint16_t ChannelCount{0};
        uint16_t BitsPerSample{16};
        uint64_t FrameCount{0};
    };

    ME_CLASS()
    class AudioClip : public Asset
    {
        ME_GENERATED_BODY(AudioClip)

    public:
        AudioClip() = default;
        ~AudioClip() override = default;

        bool IsValid() const { return m_FrameCount > 0 && !m_PcmInterleaved.empty(); }

        const AudioClipFormat& GetFormat() const { return m_Format; }
        uint64_t GetFrameCount() const { return m_FrameCount; }
        const std::vector<float>& GetPcmData() const { return m_PcmInterleaved; }
        const std::string& GetSourcePath() const { return m_SourcePath; }

        float GetDurationSeconds() const;

        static std::shared_ptr<AudioClip> CreateFromPcm(
            std::vector<float> pcmInterleaved,
            uint32_t sampleRate,
            uint16_t channelCount,
            const std::string& sourcePath = "generated/test_tone.wav");

    protected:
        friend class AudioClipLoader;

        AudioClipFormat m_Format{};
        uint64_t m_FrameCount{0};
        std::vector<float> m_PcmInterleaved;
        std::string m_SourcePath;
    };
}

#include "AudioClip.gen.h"
