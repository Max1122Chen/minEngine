#include "Texture2DLoader.h"

#include "RenderSystem.h"
#include "RHI/RHI.h"
#include "RHI/RHITexture.h"
#include "Texture.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    namespace
    {
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
    }

    std::shared_ptr<Texture2D> Texture2DLoader::CreateFromPixels(
        RHI& rhi,
        const ImagePixels& pixels,
        const std::string& debugName,
        const GUID& guid)
    {
        if (pixels.Storage != ImageStorage::UInt8 || pixels.U8 == nullptr || !pixels.IsValid())
        {
            ME_CORE_ERROR(
                "Texture2DLoader: '{}' requires 8-bit LDR pixels (HDR cubemap path is separate).",
                debugName);
            return nullptr;
        }

        const TextureFormat format = TextureFormatFromChannels(pixels.Channels);
        if (format == TextureFormat::None)
        {
            ME_CORE_ERROR(
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
        texture->m_RHITexture = rhi.CreateRHITexture2D(
            pixels.U8,
            RHITextureDesc{
                .Width = texture->m_Width,
                .Height = texture->m_Height,
                .Format = format,
                .Usage = TextureUsage::TextureBinding,
            });

        if (!texture->m_RHITexture)
        {
            ME_CORE_ERROR("Texture2DLoader: RHI failed to create texture for {}.", debugName);
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
            ME_CORE_ERROR(
                "Texture2DLoader: '{}' requires float HDR pixels.",
                debugName);
            return nullptr;
        }

        const TextureFormat format = HdrTextureFormatFromChannels(pixels.Channels);
        if (format == TextureFormat::None)
        {
            ME_CORE_ERROR(
                "Texture2DLoader: unsupported HDR channel count {} for {} (expected 3 or 4).",
                pixels.Channels,
                debugName);
            return nullptr;
        }

        std::shared_ptr<Texture2D> texture = NewObject<Texture2D>(debugName);
        texture->m_Width = static_cast<uint32_t>(pixels.Width);
        texture->m_Height = static_cast<uint32_t>(pixels.Height);
        texture->m_Channels = static_cast<uint32_t>(pixels.Channels);
        texture->m_RHITexture = rhi.CreateRHITexture2DFloat(
            pixels.F32,
            RHITextureDesc{
                .Width = texture->m_Width,
                .Height = texture->m_Height,
                .Format = format,
                .Usage = TextureUsage::TextureBinding,
            });

        if (!texture->m_RHITexture)
        {
            ME_CORE_ERROR("Texture2DLoader: RHI failed to create HDR texture for {}.", debugName);
            return nullptr;
        }

        return texture;
    }

    std::shared_ptr<Texture2D> Texture2DLoader::LoadFromAssetMeta(const AssetMeta& meta)
    {
        ImagePixels pixels;
        std::string error;
        if (!ImageLoader::Load(meta.AssetPath, pixels, true, &error))
        {
            return nullptr;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            ImageLoader::Free(pixels);
            ME_CORE_ERROR("Texture2DLoader: RHI unavailable while loading {}.", meta.AssetPath);
            return nullptr;
        }

        std::shared_ptr<Texture2D> texture =
            CreateFromPixels(*rhi, pixels, meta.AssetName, meta.Guid);
        ImageLoader::Free(pixels);
        return texture;
    }

    template<>
    std::shared_ptr<Texture2D> AssetManager::LoadAsset_Impl<Texture2D>(const AssetMeta& meta)
    {
        return Texture2DLoader::LoadFromAssetMeta(meta);
    }
}
