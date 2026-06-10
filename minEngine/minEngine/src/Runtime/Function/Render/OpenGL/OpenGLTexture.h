#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <cstdint>
#include <vector>

namespace minEngine
{
    // Internal upload helpers used by OpenGLRHITexture (not public engine API).
    struct OpenGLTextureUploadDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Layers = 1;
        TextureFormat Format = TextureFormat::None;
        TextureUsage Usage = TextureUsage::None;
    };

    class OpenGLTexture2D
    {
    public:
        OpenGLTexture2D(const unsigned char* data, OpenGLTextureUploadDesc desc);
        OpenGLTexture2D(const float* data, OpenGLTextureUploadDesc desc);
        ~OpenGLTexture2D();

        uint32_t GetID() const { return m_ID; }

    private:
        uint32_t m_ID = 0;
    };

    class OpenGLTextureCube
    {
    public:
        OpenGLTextureCube(
            const std::vector<unsigned char*>& faceData,
            OpenGLTextureUploadDesc desc,
            bool generateMipmaps = false);
        ~OpenGLTextureCube();

        uint32_t GetID() const { return m_ID; }

    private:
        uint32_t m_ID = 0;
    };

    class OpenGLTexture2DArray
    {
    public:
        OpenGLTexture2DArray(const unsigned char* data, OpenGLTextureUploadDesc desc);
        ~OpenGLTexture2DArray();

        uint32_t GetID() const { return m_ID; }

    private:
        uint32_t m_ID = 0;
    };
}
