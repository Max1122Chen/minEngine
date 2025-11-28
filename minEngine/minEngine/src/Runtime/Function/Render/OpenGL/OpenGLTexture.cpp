#include "OpenGLTexture.h"

#include "glad/glad.h"

// TODO: wrap stb_image into a ResourceManager
#include "stb_image.h"

namespace minEngine
{
    OpenGLTexture::OpenGLTexture(const char *path, uint32_t unit)
    {
        m_Unit = unit;

        glGenTextures(1, &m_ID);
        glActiveTexture(GL_TEXTURE0 + m_Unit);
        glBindTexture(GL_TEXTURE_2D, m_ID);

        // set the texture wrapping/filtering options (on the currently bound texture object)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // load and generate the texture
        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true); // flip the texture on load
        unsigned char *data = stbi_load(path, &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format;
            if (nrChannels == 1)
                format = GL_RED;
            else if (nrChannels == 3)
                format = GL_RGB;
            else if (nrChannels == 4)
                format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else
        {
            ME_CORE_ERROR("Failed to load texture: {}", path);
        }
        stbi_image_free(data);
    }
    void OpenGLTexture::Bind()
    {
        glActiveTexture(GL_TEXTURE0 + m_Unit);
        glBindTexture(GL_TEXTURE_2D, m_ID);
    }

    void OpenGLTexture::Unbind()
    {
        glBindTexture(GL_TEXTURE_2D, 0);    
    }
}