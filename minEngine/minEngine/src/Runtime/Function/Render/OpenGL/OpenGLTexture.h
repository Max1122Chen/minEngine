#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

namespace minEngine
{
    class OpenGLTexture2D : public RHITexture2D
    {
    public:

        // TODO: Add other texture creation methods (from memory, from data, etc.)
        OpenGLTexture2D(const unsigned char* data, RHITextureDesc desc, int unit = 0);
        OpenGLTexture2D(const std::string& path, int unit = 0);

        OpenGLTexture2D() = default;
        virtual ~OpenGLTexture2D() = default;

        virtual void Bind() override;
        virtual void Unbind() override;

    private:
    };

    class OpenGLTextureCube : public RHITextureCube
    {
    public:
        OpenGLTextureCube(const std::vector<unsigned char*>& faceData, RHITextureDesc desc, int unit = 0);
        OpenGLTextureCube() = default;
        virtual ~OpenGLTextureCube() = default;

        virtual void Bind() override;
        virtual void Unbind() override;
    };
}