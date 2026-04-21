#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

namespace minEngine
{
    class OpenGLTexture2D : public RHITexture2D
    {
    public:

        // TODO: Add other texture creation methods (from memory, from data, etc.)
        OpenGLTexture2D(const unsigned char* data, RHITextureDesc desc);

        OpenGLTexture2D() = default;
        virtual ~OpenGLTexture2D() override;

        virtual void Bind(int unit) override;
        virtual void Unbind() override;

    private:
    };

    class OpenGLTextureCube : public RHITextureCube
    {
    public:
        OpenGLTextureCube(const std::vector<unsigned char*>& faceData, RHITextureDesc desc);
        OpenGLTextureCube() = default;
        virtual ~OpenGLTextureCube() override;

        virtual void Bind(int unit) override;
        virtual void Unbind() override;
    };

    class OpenGLTexture2DArray : public RHITexture2DArray
    {
    public:
        OpenGLTexture2DArray(const unsigned char* data, RHITextureDesc desc);
        OpenGLTexture2DArray() = default;
        virtual ~OpenGLTexture2DArray() override;

        virtual void Bind(int unit) override;
        virtual void Unbind() override;
    };
}