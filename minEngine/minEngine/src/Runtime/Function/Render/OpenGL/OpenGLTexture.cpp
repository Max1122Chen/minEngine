#include "OpenGLTexture.h"

#include "glad/glad.h"

#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    OpenGLTexture2D::OpenGLTexture2D(const std::string& path, uint32_t unit)
        : RHITexture2D(path, unit)
    {
        glGenTextures(1, &m_ID);
        glActiveTexture(GL_TEXTURE0 + m_Unit);
        glBindTexture(GL_TEXTURE_2D, m_ID);

        // set the texture wrapping/filtering options (on the currently bound texture object)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // load and generate the texture
        AssetManager& assetManager = AssetManager::GetAssetManager();
        
        int width, height, nrChannels;
        unsigned char *data = assetManager.LoadImage(path, width, height, nrChannels);
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
            assetManager.FreeImage(data);
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