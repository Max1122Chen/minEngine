#include "Runtime/Resource/Loaders/Texture2DLoader.h"

#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/Json.h"
#include "Runtime/Resource/AssetManager.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include "Runtime/Core/Object/ObjectCast.h"

namespace minEngine
{
    namespace
    {
        constexpr const char* kMetexMagic = "METEX1\n";

        TextureFormat TextureFormatFromChannels(int channelCount)
        {
            switch (channelCount)
            {
            case 1:
                return TextureFormat::RED;
            case 3:
                return TextureFormat::RGB8;
            case 4:
                return TextureFormat::RGBA8;
            default:
                return TextureFormat::None;
            }
        }

        TextureFormat HdrTextureFormatFromChannels(int channelCount)
        {
            switch (channelCount)
            {
            case 3:
                return TextureFormat::RGB16F;
            case 4:
                return TextureFormat::RGBA16F;
            default:
                return TextureFormat::None;
            }
        }

        bool EndsWithIgnoreCase(std::string_view value, std::string_view suffix)
        {
            if (value.size() < suffix.size())
            {
                return false;
            }
            for (size_t i = 0; i < suffix.size(); ++i)
            {
                const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(
                    value[value.size() - suffix.size() + i])));
                const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
                if (a != b)
                {
                    return false;
                }
            }
            return true;
        }

        bool IsLegacyImagePath(std::string_view path)
        {
            return EndsWithIgnoreCase(path, ".png") || EndsWithIgnoreCase(path, ".jpg")
                || EndsWithIgnoreCase(path, ".jpeg");
        }

        bool IsMetexPath(std::string_view path)
        {
            return EndsWithIgnoreCase(path, ".metex");
        }

        std::string CookedFormatFromChannels(int channels)
        {
            switch (channels)
            {
            case 1:
                return "R8";
            case 3:
                return "RGB8";
            case 4:
                return "RGBA8";
            default:
                return "RGB8";
            }
        }

        int ChannelsFromCookedFormat(std::string_view format)
        {
            if (format == "R8")
            {
                return 1;
            }
            if (format == "RGBA8")
            {
                return 4;
            }
            return 3;
        }
    }

    std::shared_ptr<Texture2D> Texture2DLoader::CreateFromPixels(
        RHI& rhi,
        const ImagePixels& pixels,
        const std::string& debugName,
        const GUID& guid)
    {
        if (pixels.Storage != ImageStorage::UInt8 || pixels.U8 == nullptr || !pixels.IsValid())
        {
            ME_LOG(
                LogAsset,
                Error,
                "Texture2DLoader: '{}' requires 8-bit LDR pixels (HDR cubemap path is separate).",
                debugName);
            return nullptr;
        }

        const TextureFormat format = TextureFormatFromChannels(pixels.Channels);
        if (format == TextureFormat::None)
        {
            ME_LOG(
                LogAsset,
                Error,
                "Texture2DLoader: unsupported channel count {} for {} (expected 1, 3, or 4).",
                pixels.Channels,
                debugName);
            return nullptr;
        }

        std::shared_ptr<Texture2D> texture = NewObject<Texture2D>(debugName);
        texture->SetGuid(guid);
        texture->m_Width = static_cast<uint32_t>(pixels.Width);
        texture->m_Height = static_cast<uint32_t>(pixels.Height);
        texture->m_Channels = static_cast<uint32_t>(pixels.Channels);
        texture->m_RHITexture = rhi.RHICreateTexture2D(
            MakeTexture2DBindingDesc(texture->m_Width, texture->m_Height, format),
            pixels.U8);

        if (!texture->m_RHITexture)
        {
            ME_LOG(LogAsset, Error, "Texture2DLoader: RHI failed to create texture for {}.", debugName);
            return nullptr;
        }

        return texture;
    }

    std::shared_ptr<Texture2D> Texture2DLoader::CreateFromHdrPixels(
        RHI& rhi,
        const ImagePixels& pixels,
        const std::string& debugName)
    {
        if (pixels.Storage != ImageStorage::Float32 || pixels.F32 == nullptr || !pixels.IsValid())
        {
            ME_LOG(LogAsset, Error, "Texture2DLoader: '{}' requires float HDR pixels.", debugName);
            return nullptr;
        }

        const TextureFormat format = HdrTextureFormatFromChannels(pixels.Channels);
        if (format == TextureFormat::None)
        {
            ME_LOG(
                LogAsset,
                Error,
                "Texture2DLoader: unsupported HDR channel count {} for {} (expected 3 or 4).",
                pixels.Channels,
                debugName);
            return nullptr;
        }

        std::shared_ptr<Texture2D> texture = NewObject<Texture2D>(debugName);
        texture->m_Width = static_cast<uint32_t>(pixels.Width);
        texture->m_Height = static_cast<uint32_t>(pixels.Height);
        texture->m_Channels = static_cast<uint32_t>(pixels.Channels);
        texture->m_RHITexture = rhi.RHICreateTexture2D(
            MakeTexture2DBindingDesc(texture->m_Width, texture->m_Height, format),
            pixels.F32);

        if (!texture->m_RHITexture)
        {
            ME_LOG(LogAsset, Error, "Texture2DLoader: RHI failed to create HDR texture for {}.", debugName);
            return nullptr;
        }

        return texture;
    }

    bool Texture2DLoader::ApplyChannelSemantics(
        ImagePixels& inOutPixels,
        const Texture2DCookSettings& settings,
        std::string* outError)
    {
        if (inOutPixels.Storage != ImageStorage::UInt8 || !inOutPixels.IsValid())
        {
            if (outError)
            {
                *outError = "ApplyChannelSemantics requires valid U8 LDR pixels";
            }
            return false;
        }

        const bool singleChannel =
            settings.ChannelSemantics == "SingleChannel" || settings.ChannelSemantics == "Mask";

        if (singleChannel)
        {
            if (inOutPixels.Channels == 1)
            {
                return true;
            }

            const int pixelCount = inOutPixels.Width * inOutPixels.Height;
            auto* out = static_cast<unsigned char*>(std::malloc(static_cast<size_t>(pixelCount)));
            if (out == nullptr)
            {
                if (outError)
                {
                    *outError = "out of memory expanding SingleChannel cook";
                }
                return false;
            }
            for (int i = 0; i < pixelCount; ++i)
            {
                out[i] = inOutPixels.U8[i * inOutPixels.Channels];
            }
            const int keptWidth = inOutPixels.Width;
            const int keptHeight = inOutPixels.Height;
            ImageLoader::Free(inOutPixels);
            inOutPixels.U8 = out;
            inOutPixels.Width = keptWidth;
            inOutPixels.Height = keptHeight;
            inOutPixels.Channels = 1;
            inOutPixels.Storage = ImageStorage::UInt8;
            return true;
        }

        // Color: expand 1ch -> RGB8 R=G=B
        if (inOutPixels.Channels == 1)
        {
            const int pixelCount = inOutPixels.Width * inOutPixels.Height;
            auto* out = static_cast<unsigned char*>(std::malloc(static_cast<size_t>(pixelCount) * 3u));
            if (out == nullptr)
            {
                if (outError)
                {
                    *outError = "out of memory expanding Color cook";
                }
                return false;
            }
            for (int i = 0; i < pixelCount; ++i)
            {
                const unsigned char v = inOutPixels.U8[i];
                out[i * 3 + 0] = v;
                out[i * 3 + 1] = v;
                out[i * 3 + 2] = v;
            }
            const int keptWidth = inOutPixels.Width;
            const int keptHeight = inOutPixels.Height;
            ImageLoader::Free(inOutPixels);
            inOutPixels.U8 = out;
            inOutPixels.Width = keptWidth;
            inOutPixels.Height = keptHeight;
            inOutPixels.Channels = 3;
            inOutPixels.Storage = ImageStorage::UInt8;
            return true;
        }

        if (inOutPixels.Channels == 3 || inOutPixels.Channels == 4)
        {
            return true;
        }

        if (outError)
        {
            *outError = "unsupported source channel count for Color cook";
        }
        return false;
    }

    bool Texture2DLoader::WriteCookedFile(
        const std::string& absolutePath,
        const Texture2DCookSettings& settings,
        const ImagePixels& pixels,
        std::string* outError)
    {
        if (pixels.Storage != ImageStorage::UInt8 || !pixels.IsValid())
        {
            if (outError)
            {
                *outError = "WriteCookedFile requires valid U8 pixels";
            }
            return false;
        }

        Texture2DCookSettings written = settings;
        written.CookedFormat = CookedFormatFromChannels(pixels.Channels);

        Json header = Json::object();
        header["version"] = written.Version;
        header["width"] = pixels.Width;
        header["height"] = pixels.Height;
        header["channels"] = pixels.Channels;
        header["cookedFormat"] = written.CookedFormat;
        header["channelSemantics"] = written.ChannelSemantics;
        header["sRGB"] = written.bSRGB;
        header["wrapU"] = written.WrapU;
        header["wrapV"] = written.WrapV;
        header["filter"] = written.Filter;

        const std::string headerText = header.dump();
        const size_t byteCount =
            static_cast<size_t>(pixels.Width) * static_cast<size_t>(pixels.Height)
            * static_cast<size_t>(pixels.Channels);

        const std::filesystem::path outPath(absolutePath);
        if (outPath.has_parent_path())
        {
            std::error_code createError;
            std::filesystem::create_directories(outPath.parent_path(), createError);
            if (createError)
            {
                if (outError)
                {
                    *outError = "failed to create .metex parent directory: " + createError.message();
                }
                return false;
            }
        }

        std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            if (outError)
            {
                *outError = "failed to open .metex for write: " + absolutePath;
            }
            return false;
        }

        out.write(kMetexMagic, static_cast<std::streamsize>(std::strlen(kMetexMagic)));
        out.write(headerText.data(), static_cast<std::streamsize>(headerText.size()));
        out.put('\n');
        out.write(reinterpret_cast<const char*>(pixels.U8), static_cast<std::streamsize>(byteCount));
        if (!out)
        {
            if (outError)
            {
                *outError = "failed while writing .metex payload";
            }
            return false;
        }
        return true;
    }

    bool Texture2DLoader::ReadCookedFile(
        const std::string& absolutePath,
        Texture2DCookSettings& outSettings,
        ImagePixels& outPixels,
        std::string* outError)
    {
        ImageLoader::Free(outPixels);

        std::ifstream in(absolutePath, std::ios::binary);
        if (!in)
        {
            if (outError)
            {
                *outError = "failed to open .metex: " + absolutePath;
            }
            return false;
        }

        char magic[8] = {};
        in.read(magic, 7);
        if (!in || std::strncmp(magic, kMetexMagic, 7) != 0)
        {
            if (outError)
            {
                *outError = "invalid .metex magic";
            }
            return false;
        }

        std::string headerLine;
        if (!std::getline(in, headerLine))
        {
            if (outError)
            {
                *outError = "failed to read .metex header";
            }
            return false;
        }

        Json header;
        try
        {
            header = Json::parse(headerLine);
        }
        catch (const std::exception& ex)
        {
            if (outError)
            {
                *outError = std::string("failed to parse .metex header: ") + ex.what();
            }
            return false;
        }

        outSettings.Version = header.value("version", 1);
        outSettings.ChannelSemantics = header.value("channelSemantics", "Color");
        outSettings.CookedFormat = header.value("cookedFormat", "RGB8");
        outSettings.bSRGB = header.value("sRGB", true);
        outSettings.WrapU = header.value("wrapU", "Repeat");
        outSettings.WrapV = header.value("wrapV", "Repeat");
        outSettings.Filter = header.value("filter", "Linear");

        const int width = header.value("width", 0);
        const int height = header.value("height", 0);
        int channels = header.value("channels", 0);
        if (channels <= 0)
        {
            channels = ChannelsFromCookedFormat(outSettings.CookedFormat);
        }
        if (width <= 0 || height <= 0 || channels <= 0)
        {
            if (outError)
            {
                *outError = "invalid .metex dimensions";
            }
            return false;
        }

        const size_t byteCount =
            static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels);
        auto* bytes = static_cast<unsigned char*>(std::malloc(byteCount));
        if (bytes == nullptr)
        {
            if (outError)
            {
                *outError = "out of memory reading .metex";
            }
            return false;
        }
        in.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(byteCount));
        if (!in || static_cast<size_t>(in.gcount()) != byteCount)
        {
            std::free(bytes);
            if (outError)
            {
                *outError = "truncated .metex pixel payload";
            }
            return false;
        }

        outPixels.Storage = ImageStorage::UInt8;
        outPixels.U8 = bytes;
        outPixels.Width = width;
        outPixels.Height = height;
        outPixels.Channels = channels;
        return true;
    }

    bool Texture2DLoader::ReadCookSettings(
        const std::string& absolutePath,
        Texture2DCookSettings& outSettings,
        std::string* outError)
    {
        ImagePixels unused;
        const bool ok = ReadCookedFile(absolutePath, outSettings, unused, outError);
        ImageLoader::Free(unused);
        return ok;
    }

    bool Texture2DLoader::TryGetCookedPixels(
        const AssetMeta& meta,
        ImagePixels& outPixels,
        std::string* outError)
    {
        ImageLoader::Free(outPixels);
        const std::string absolutePath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        if (IsMetexPath(meta.AssetPath) || IsMetexPath(absolutePath))
        {
            Texture2DCookSettings settings;
            return ReadCookedFile(absolutePath, settings, outPixels, outError);
        }

        if (IsLegacyImagePath(meta.AssetPath) || IsLegacyImagePath(absolutePath))
        {
            ME_LOG(
                LogAsset,
                Warn,
                "Texture2DLoader: loading legacy image asset '{}' (migrate to .metex).",
                meta.AssetPath);
            return ImageLoader::Load(absolutePath, outPixels, true, outError);
        }

        if (outError)
        {
            *outError = "unsupported Texture2D asset path for cooked pixels: " + meta.AssetPath;
        }
        return false;
    }

    std::shared_ptr<Texture2D> Texture2DLoader::LoadFromAssetMeta(const AssetMeta& meta)
    {
        ImagePixels pixels;
        std::string error;
        if (!TryGetCookedPixels(meta, pixels, &error))
        {
            ME_LOG(LogAsset, Error, "Texture2DLoader: failed to load '{}': {}", meta.AssetPath, error);
            return nullptr;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            ImageLoader::Free(pixels);
            ME_LOG(LogAsset, Error, "Texture2DLoader: RHI unavailable while loading {}.", meta.AssetPath);
            return nullptr;
        }

        std::shared_ptr<Texture2D> texture =
            CreateFromPixels(*rhi, pixels, meta.AssetName, meta.Guid);
        ImageLoader::Free(pixels);
        return texture;
    }

    bool Texture2DLoader::SaveHandler(
        const AssetManager& manager,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const auto* texture = Cast<Texture2D>(&asset);
        if (texture == nullptr)
        {
            outError = "SaveHandler: expected Texture2D";
            return false;
        }

        // Runtime GPU-only saves are not supported; keep existing .metex if present.
        Texture2DCookSettings settings;
        ImagePixels pixels;
        const std::string absolutePath = manager.ResolveAssetAbsolutePath(meta.AssetPath).string();
        if (ReadCookedFile(absolutePath, settings, pixels, &outError))
        {
            ImageLoader::Free(pixels);
            return true;
        }

        outError =
            "Texture2D Save requires an existing cooked .metex (use Import/Recook to rewrite pixels).";
        return false;
    }

    std::shared_ptr<Asset> Texture2DLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        outError.clear();
        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".metex");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            outError = "failed to create directory: " + createError.message();
            return nullptr;
        }

        ImagePixels pixels;
        pixels.Storage = ImageStorage::UInt8;
        pixels.Width = 1;
        pixels.Height = 1;
        pixels.Channels = 3;
        pixels.U8 = static_cast<unsigned char*>(std::malloc(3));
        if (pixels.U8 == nullptr)
        {
            outError = "out of memory";
            return nullptr;
        }
        pixels.U8[0] = 255;
        pixels.U8[1] = 255;
        pixels.U8[2] = 255;

        Texture2DCookSettings settings;
        settings.ChannelSemantics = "Color";
        settings.bSRGB = true;
        std::string writeError;
        if (!WriteCookedFile(absolutePath.string(), settings, pixels, &writeError))
        {
            ImageLoader::Free(pixels);
            outError = writeError;
            return nullptr;
        }
        ImageLoader::Free(pixels);

        const std::string textureName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        RHI* rhi = RenderSystem::Get().GetRHI();
        std::shared_ptr<Texture2D> texture;
        if (rhi != nullptr)
        {
            ImagePixels solid{};
            solid.Storage = ImageStorage::UInt8;
            solid.Width = 1;
            solid.Height = 1;
            solid.Channels = 3;
            solid.U8 = static_cast<unsigned char*>(std::malloc(3));
            solid.U8[0] = solid.U8[1] = solid.U8[2] = 255;
            texture = CreateFromPixels(*rhi, solid, textureName, assetGuid);
            ImageLoader::Free(solid);
        }
        else
        {
            texture = NewObject<Texture2D>(textureName, nullptr, assetGuid);
            texture->m_Width = 1;
            texture->m_Height = 1;
            texture->m_Channels = 3;
        }

        if (!texture)
        {
            outError = "failed to create Texture2D object";
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);
        AssetMeta meta = manager.RegisterAsset(relativePath, "Texture2D", &assetGuid);
        if (meta.AssetPath.empty())
        {
            outError = "RegisterAsset failed";
            return nullptr;
        }
        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));
        manager.CacheCreatedAsset(meta.AssetPath, texture);
        return std::static_pointer_cast<Asset>(texture);
    }
}
