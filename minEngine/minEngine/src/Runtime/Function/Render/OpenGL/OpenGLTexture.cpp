#include "OpenGLTexture.h"

#include "glad/glad.h"

#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    OpenGLTexture2D::OpenGLTexture2D(const unsigned char *data, RHITextureDesc desc, int unit)
    {
        m_Unit = unit;
        glGenTextures(1, &m_ID);
        glActiveTexture(GL_TEXTURE0 + m_Unit);
        glBindTexture(GL_TEXTURE_2D, m_ID);

        // set the texture wrapping/filtering options (on the currently bound texture object). TODO: make these configurable
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLint internalFormat = 0;
        GLenum dataFormat = 0;
        GLenum dataType = GL_UNSIGNED_BYTE;

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
        else if (desc.Format == TextureFormat::DEPTH24STENCIL8)
        {
            internalFormat = GL_DEPTH24_STENCIL8;
            dataFormat = GL_DEPTH_STENCIL;
            dataType = GL_UNSIGNED_INT_24_8;
        }

        // Apply Usage overrides or defaults if Format was not sufficient
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

        bool isDepthStencil = (desc.Format == TextureFormat::DEPTH24STENCIL8) ||
                              (desc.Usage == TextureUsage::Depth) ||
                              (desc.Usage == TextureUsage::Stencil) ||
                              (desc.Usage == TextureUsage::DepthStencil);

        if (isDepthStencil)
        {
            // Depth/Stencil textures usually don't support mipmaps or we don't restart them often
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        if (internalFormat != 0 && desc.Width > 0 && desc.Height > 0)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc.Width, desc.Height, 0, dataFormat, dataType, data);
            
            if (!isDepthStencil)
            {
               glGenerateMipmap(GL_TEXTURE_2D);
            }
        }
    }

    // TODO: move these logic to material
    void OpenGLTexture2D::Bind()
    {
        glActiveTexture(GL_TEXTURE0 + m_Unit);
        glBindTexture(GL_TEXTURE_2D, m_ID);
    }

    void OpenGLTexture2D::Unbind()
    {
        glBindTexture(GL_TEXTURE_2D, 0);    
    }

    OpenGLTextureCube::OpenGLTextureCube(const std::vector<unsigned char *> &faceData, RHITextureDesc desc, int unit)
    {
        m_Unit = unit;
        glGenTextures(1, &m_ID);
        glActiveTexture(GL_TEXTURE0 + m_Unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);

        // set the texture wrapping/filtering options (on the currently bound texture object). TODO: make these configurable
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        for (unsigned int i = 0; i < 6; i++)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, desc.Width, desc.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, faceData[i]);
        }
    }

    void OpenGLTextureCube::Bind()
    {
        glActiveTexture(GL_TEXTURE0 + m_Unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);
    }

    void OpenGLTextureCube::Unbind()
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
}