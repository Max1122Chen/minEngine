#include "OpenGLTexture.h"

#include "glad/glad.h"

#include "Runtime/Core/Log/LogSystem.h"

namespace minEngine
{
    namespace
    {
        void ResolveOpenGLTextureFormat(const OpenGLTextureUploadDesc& desc, GLint& internalFormat, GLenum& dataFormat, GLenum& dataType)
        {
            internalFormat = 0;
            dataFormat = 0;
            dataType = GL_UNSIGNED_BYTE;

            if (desc.Format == TextureFormat::RED)
            {
                internalFormat = GL_R8;
                dataFormat = GL_RED;
            }
            else if (desc.Format == TextureFormat::RGB8)
            {
                internalFormat = GL_RGB8;
                dataFormat = GL_RGB;
            }
            else if (desc.Format == TextureFormat::RGBA8)
            {
                internalFormat = GL_RGBA8;
                dataFormat = GL_RGBA;
            }
            else if (desc.Format == TextureFormat::RGB16F)
            {
                internalFormat = GL_RGB16F;
                dataFormat = GL_RGB;
                dataType = GL_FLOAT;
            }
            else if (desc.Format == TextureFormat::RGBA16F)
            {
                internalFormat = GL_RGBA16F;
                dataFormat = GL_RGBA;
                dataType = GL_FLOAT;
            }
            else if (desc.Format == TextureFormat::DEPTH16)
            {
                internalFormat = GL_DEPTH_COMPONENT16;
                dataFormat = GL_DEPTH_COMPONENT;
                dataType = GL_UNSIGNED_SHORT;
            }
            else if (desc.Format == TextureFormat::DEPTH24)
            {
                internalFormat = GL_DEPTH_COMPONENT24;
                dataFormat = GL_DEPTH_COMPONENT;
                dataType = GL_UNSIGNED_INT;
            }
            else if (desc.Format == TextureFormat::DEPTH32)
            {
                internalFormat = GL_DEPTH_COMPONENT32;
                dataFormat = GL_DEPTH_COMPONENT;
                dataType = GL_UNSIGNED_INT;
            }
            else if (desc.Format == TextureFormat::DEPTH24STENCIL8)
            {
                internalFormat = GL_DEPTH24_STENCIL8;
                dataFormat = GL_DEPTH_STENCIL;
                dataType = GL_UNSIGNED_INT_24_8;
            }

            if (internalFormat == 0)
            {
                if (desc.Usage == TextureUsage::Depth)
                {
                    internalFormat = GL_DEPTH_COMPONENT;
                    dataFormat = GL_DEPTH_COMPONENT;
                }
                else if (desc.Usage == TextureUsage::Stencil)
                {
                    internalFormat = GL_STENCIL_INDEX;
                    dataFormat = GL_STENCIL_INDEX;
                }
                else if (desc.Usage == TextureUsage::DepthStencil)
                {
                    internalFormat = GL_DEPTH_STENCIL;
                    dataFormat = GL_DEPTH_STENCIL;
                    dataType = GL_UNSIGNED_INT_24_8;
                }
            }
        }

        bool IsDepthLikeTexture(const OpenGLTextureUploadDesc& desc)
        {
            return (desc.Format == TextureFormat::DEPTH16) ||
                   (desc.Format == TextureFormat::DEPTH24) ||
                   (desc.Format == TextureFormat::DEPTH32) ||
                   (desc.Format == TextureFormat::DEPTH24STENCIL8) ||
                   (desc.Usage == TextureUsage::Depth) ||
                   (desc.Usage == TextureUsage::DepthStencil);
        }

        bool IsFloatColorTexture(const OpenGLTextureUploadDesc& desc)
        {
            return desc.Format == TextureFormat::RGB16F || desc.Format == TextureFormat::RGBA16F;
        }

        void Configure2DTextureSampling(GLenum target, const OpenGLTextureUploadDesc& desc, bool generateMipmaps)
        {
            if (IsDepthLikeTexture(desc))
            {
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor);
                return;
            }

            if (IsFloatColorTexture(desc))
            {
                glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                return;
            }

            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            (void)generateMipmaps;
        }
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        if (m_ID != 0)
        {
            glDeleteTextures(1, &m_ID);
            m_ID = 0;
        }
    }

    OpenGLTexture2D::OpenGLTexture2D(const unsigned char *data, OpenGLTextureUploadDesc desc)
    {
        glGenTextures(1, &m_ID);
        glBindTexture(GL_TEXTURE_2D, m_ID);

        GLint internalFormat = 0;
        GLenum dataFormat = 0;
        GLenum dataType = GL_UNSIGNED_BYTE;
        ResolveOpenGLTextureFormat(desc, internalFormat, dataFormat, dataType);

        const bool isDepthStencil = IsDepthLikeTexture(desc);
        Configure2DTextureSampling(GL_TEXTURE_2D, desc, true);

        if (internalFormat != 0 && desc.Width > 0 && desc.Height > 0)
        {
            GLint previousUnpackAlignment = 4;
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc.Width, desc.Height, 0, dataFormat, dataType, data);

            glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);

            if (!isDepthStencil)
            {
                glGenerateMipmap(GL_TEXTURE_2D);
            }
        }
    }

    OpenGLTexture2D::OpenGLTexture2D(const float* data, OpenGLTextureUploadDesc desc)
    {
        glGenTextures(1, &m_ID);
        glBindTexture(GL_TEXTURE_2D, m_ID);

        GLint internalFormat = 0;
        GLenum dataFormat = 0;
        GLenum dataType = GL_UNSIGNED_BYTE;
        ResolveOpenGLTextureFormat(desc, internalFormat, dataFormat, dataType);

        if (!IsFloatColorTexture(desc) || internalFormat == 0)
        {
            ME_CORE_ERROR("OpenGLTexture2D(float): unsupported format.");
            glDeleteTextures(1, &m_ID);
            m_ID = 0;
            return;
        }

        Configure2DTextureSampling(GL_TEXTURE_2D, desc, false);

        if (desc.Width > 0 && desc.Height > 0)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc.Width, desc.Height, 0, dataFormat, dataType, data);
        }
    }

    OpenGLTextureCube::OpenGLTextureCube(
        const std::vector<unsigned char*>& faceData,
        OpenGLTextureUploadDesc desc,
        bool generateMipmaps)
    {
        if (desc.Width == 0 || desc.Height == 0)
        {
            ME_CORE_ERROR("OpenGLTextureCube: Width/Height must be > 0.");
            return;
        }

        if (faceData.size() < 6)
        {
            ME_CORE_ERROR(
                "OpenGLTextureCube: expected 6 face pointers, got {}.",
                faceData.size());
            return;
        }

        glGenTextures(1, &m_ID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        GLint internalFormat = 0;
        GLenum dataFormat = 0;
        GLenum dataType = GL_UNSIGNED_BYTE;
        ResolveOpenGLTextureFormat(desc, internalFormat, dataFormat, dataType);

        const bool isDepthLike = IsDepthLikeTexture(desc);
        if (!isDepthLike && internalFormat == 0)
        {
            ME_CORE_ERROR("OpenGLTextureCube: unsupported color format for cubemap.");
            glDeleteTextures(1, &m_ID);
            m_ID = 0;
            return;
        }

        if (isDepthLike)
        {
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else if (generateMipmaps)
        {
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        GLint previousUnpackAlignment = 4;
        if (!isDepthLike)
        {
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }

        for (unsigned int faceIndex = 0; faceIndex < 6; faceIndex++)
        {
            const unsigned char* facePixels = faceData[faceIndex];
            if (!isDepthLike && facePixels == nullptr && !IsFloatColorTexture(desc))
            {
                ME_CORE_ERROR("OpenGLTextureCube: color face {} is null.", faceIndex);
                continue;
            }

            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
                0,
                internalFormat,
                desc.Width,
                desc.Height,
                0,
                dataFormat,
                dataType,
                facePixels);
        }

        if (!isDepthLike)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
            if (generateMipmaps && m_ID != 0)
            {
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            }
        }

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

    OpenGLTextureCube::~OpenGLTextureCube()
    {
        if (m_ID != 0)
        {
            glDeleteTextures(1, &m_ID);
            m_ID = 0;
        }
    }

    OpenGLTexture2DArray::OpenGLTexture2DArray(const unsigned char *data, OpenGLTextureUploadDesc desc)
    {
        const uint32_t layerCount = (desc.Layers == 0) ? 1u : desc.Layers;

        glGenTextures(1, &m_ID);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_ID);

        GLint internalFormat = 0;
        GLenum dataFormat = 0;
        GLenum dataType = GL_UNSIGNED_BYTE;
        ResolveOpenGLTextureFormat(desc, internalFormat, dataFormat, dataType);
        const bool isDepthLike = IsDepthLikeTexture(desc);

        if (isDepthLike)
        {
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
            glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
        }
        else
        {
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        if (internalFormat != 0 && desc.Width > 0 && desc.Height > 0)
        {
            glTexImage3D(
                GL_TEXTURE_2D_ARRAY,
                0,
                internalFormat,
                static_cast<GLsizei>(desc.Width),
                static_cast<GLsizei>(desc.Height),
                static_cast<GLsizei>(layerCount),
                0,
                dataFormat,
                dataType,
                data);

            if (!isDepthLike)
            {
                glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
            }
        }
    }

    OpenGLTexture2DArray::~OpenGLTexture2DArray()
    {
        if (m_ID != 0)
        {
            glDeleteTextures(1, &m_ID);
            m_ID = 0;
        }
    }
}