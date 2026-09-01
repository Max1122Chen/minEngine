#pragma once

#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/AudioClip.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class AudioClip;

    class AudioClipLoader
    {
    public:
        static std::shared_ptr<AudioClip> Load(const AssetMeta& meta, std::string* outError = nullptr);

    private:
        static bool DecodeFileToPcm(
            const std::filesystem::path& absolutePath,
            std::vector<float>& outPcmInterleaved,
            AudioClipFormat& outFormat,
            std::string* outError);
    };
}
