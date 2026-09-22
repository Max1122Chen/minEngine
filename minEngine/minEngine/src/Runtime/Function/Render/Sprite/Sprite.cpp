#include "Sprite.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace minEngine
{
    Sprite::~Sprite()
    {
        ClearAuthoringPixels();
    }

    void Sprite::SetSourceTexture(const std::shared_ptr<Texture2D>& texture)
    {
        if (m_SourceTexture == texture)
        {
            return;
        }

        m_SourceTexture = texture;
        ClearBaked();
    }

    void Sprite::SetSourceRectPx(const Vector4& rectPx)
    {
        if (m_SourceRectPx == rectPx)
        {
            return;
        }

        m_SourceRectPx = rectPx;
        ClearBaked();
    }

    Texture2D* Sprite::GetRuntimeTexture() const
    {
        if (m_BakedTexture)
        {
            return m_BakedTexture.get();
        }

        return m_SourceTexture.get();
    }

    std::shared_ptr<Texture2D> Sprite::GetRuntimeTextureShared() const
    {
        if (m_BakedTexture)
        {
            return m_BakedTexture;
        }

        return m_SourceTexture;
    }

    Vector4 Sprite::MakeFullTextureRect(const Texture2D& texture)
    {
        return Vector4(
            0.0f,
            0.0f,
            static_cast<float>(texture.GetWidth()),
            static_cast<float>(texture.GetHeight()));
    }

    Vector4 Sprite::ResolveSourceRectPx() const
    {
        if (!m_SourceTexture)
        {
            return m_SourceRectPx;
        }

        if (m_SourceRectPx.z <= 0.0f || m_SourceRectPx.w <= 0.0f)
        {
            return MakeFullTextureRect(*m_SourceTexture);
        }

        return m_SourceRectPx;
    }

    Vector4 Sprite::PixelRectToUvRectTopLeft(
        const Vector4& rectPx,
        uint32_t textureWidth,
        uint32_t textureHeight)
    {
        if (textureWidth == 0 || textureHeight == 0 || rectPx.z <= 0.0f || rectPx.w <= 0.0f)
        {
            return Vector4(0.0f, 0.0f, 1.0f, 1.0f);
        }

        const float tw = static_cast<float>(textureWidth);
        const float th = static_cast<float>(textureHeight);
        const float x = rectPx.x;
        const float y = rectPx.y;
        const float w = rectPx.z;
        const float h = rectPx.w;

        // Authoring: y=0 at image top. SpriteQuadMesh: v=0 at bottom (OpenGL).
        const float u0 = x / tw;
        const float u1 = (x + w) / tw;
        const float v0 = 1.0f - (y + h) / th;
        const float v1 = 1.0f - y / th;
        return Vector4(u0, v0, u1, v1);
    }

    Vector4 Sprite::GetUVRectNormalized() const
    {
        Texture2D* runtimeTexture = GetRuntimeTexture();
        if (runtimeTexture == nullptr)
        {
            return Vector4(0.0f, 0.0f, 1.0f, 1.0f);
        }

        const Vector4 rectPx = m_BakedTexture ? m_BakedRectPx : ResolveSourceRectPx();
        return PixelRectToUvRectTopLeft(rectPx, runtimeTexture->GetWidth(), runtimeTexture->GetHeight());
    }

    void Sprite::ClearBaked()
    {
        m_BakedTexture.reset();
        m_BakedRectPx = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    void Sprite::SetBaked(const std::shared_ptr<Texture2D>& texture, const Vector4& rectPx)
    {
        m_BakedTexture = texture;
        m_BakedRectPx = rectPx;
    }

    void Sprite::ConfigureFromTexture(const std::shared_ptr<Texture2D>& texture, const Vector4& sourceRectPx)
    {
        SetSourceTexture(texture);
        SetSourceRectPx(sourceRectPx);
    }

    bool Sprite::SetAuthoringPixels(const ImagePixels& pixels)
    {
        if (pixels.Storage != ImageStorage::UInt8 || !pixels.IsValid() || pixels.U8 == nullptr)
        {
            ME_LOG(LogAsset, Error, "Sprite::SetAuthoringPixels requires valid U8 LDR pixels.");
            return false;
        }

        const size_t byteCount =
            static_cast<size_t>(pixels.Width) * static_cast<size_t>(pixels.Height)
            * static_cast<size_t>(pixels.Channels);
        m_AuthoringPixels.resize(byteCount);
        std::memcpy(m_AuthoringPixels.data(), pixels.U8, byteCount);
        m_AuthoringWidth = pixels.Width;
        m_AuthoringHeight = pixels.Height;
        m_AuthoringChannels = pixels.Channels;
        return true;
    }

    void Sprite::ClearAuthoringPixels()
    {
        m_AuthoringPixels.clear();
        m_AuthoringWidth = 0;
        m_AuthoringHeight = 0;
        m_AuthoringChannels = 0;
    }

    bool Sprite::TryCopyAuthoringPixels(ImagePixels& outPixels) const
    {
        ImageLoader::Free(outPixels);
        if (m_AuthoringPixels.empty() || m_AuthoringWidth <= 0 || m_AuthoringHeight <= 0 || m_AuthoringChannels <= 0)
        {
            return false;
        }

        const size_t byteCount = m_AuthoringPixels.size();
        unsigned char* buffer = static_cast<unsigned char*>(std::malloc(byteCount));
        if (buffer == nullptr)
        {
            return false;
        }

        std::memcpy(buffer, m_AuthoringPixels.data(), byteCount);
        outPixels.Storage = ImageStorage::UInt8;
        outPixels.U8 = buffer;
        outPixels.F32 = nullptr;
        outPixels.Width = m_AuthoringWidth;
        outPixels.Height = m_AuthoringHeight;
        outPixels.Channels = m_AuthoringChannels;
        return true;
    }
}
