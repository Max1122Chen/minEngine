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

        if(data)
        {
            GLenum format;
            if (desc.Format == TextureFormat::RED)
                format = GL_RED;
            else if (desc.Format == TextureFormat::RGB8)
                format = GL_RGB;
            else if (desc.Format == TextureFormat::RGBA8)
                format = GL_RGBA;
            else if (desc.Format == TextureFormat::DEPTH24STENCIL8)
                format = GL_DEPTH24_STENCIL8;

            glTexImage2D(GL_TEXTURE_2D, 0, format, desc.Width, desc.Height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
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
}