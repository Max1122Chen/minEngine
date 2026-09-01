#include "Runtime/Resource/Loaders/AudioClipLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AudioClip.h"

#include "miniaudio.h"

#include <algorithm>
#include <cmath>

namespace minEngine
{
    bool AudioClipLoader::DecodeFileToPcm(
        const std::filesystem::path& absolutePath,
        std::vector<float>& outPcmInterleaved,
        AudioClipFormat& outFormat,
        std::string* outError)
    {
        outPcmInterleaved.clear();
        outFormat = {};

        ma_decoder decoder;
        ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
        const ma_result initResult = ma_decoder_init_file(absolutePath.string().c_str(), &decoderConfig, &decoder);
        if (initResult != MA_SUCCESS)
        {
            const std::string message = "AudioClipLoader: failed to open audio file: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        ma_uint64 totalFrames = 0;
        if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS || totalFrames == 0)
        {
            ma_decoder_uninit(&decoder);
            const std::string message = "AudioClipLoader: failed to query frame count: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        const ma_uint32 channelCount = decoder.outputChannels;
        const ma_uint32 sampleRate = decoder.outputSampleRate;
        outPcmInterleaved.resize(static_cast<size_t>(totalFrames * channelCount));

        ma_uint64 framesRead = 0;
        const ma_result readResult = ma_decoder_read_pcm_frames(
            &decoder,
            outPcmInterleaved.data(),
            totalFrames,
            &framesRead);
        ma_decoder_uninit(&decoder);

        if (readResult != MA_SUCCESS || framesRead == 0)
        {
            outPcmInterleaved.clear();
            const std::string message = "AudioClipLoader: failed to decode PCM: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        outPcmInterleaved.resize(static_cast<size_t>(framesRead * channelCount));
        outFormat.SampleRate = sampleRate;
        outFormat.ChannelCount = static_cast<uint16_t>(channelCount);
        outFormat.BitsPerSample = 32;
        outFormat.FrameCount = framesRead;
        return true;
    }

    std::shared_ptr<AudioClip> AudioClipLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        const std::filesystem::path absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath);

        std::vector<float> pcm;
        AudioClipFormat format;
        std::string error;
        if (!DecodeFileToPcm(absoluteAssetPath, pcm, format, &error))
        {
            if (outError != nullptr)
            {
                *outError = error;
            }
            return nullptr;
        }

        std::shared_ptr<AudioClip> clip = NewObject<AudioClip>(meta.AssetName, nullptr, meta.Guid);
        clip->m_Format = format;
        clip->m_FrameCount = format.FrameCount;
        clip->m_PcmInterleaved = std::move(pcm);
        clip->m_SourcePath = meta.AssetPath;
        return clip;
    }

    template<>
    std::shared_ptr<AudioClip> AssetManager::LoadAsset_Impl<AudioClip>(const AssetMeta& meta)
    {
        return AudioClipLoader::Load(meta, nullptr);
    }
}
