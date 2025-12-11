#include "Material.h"

namespace minEngine
{
    void Material::BindTextures() const
    {
        // TODO: bind other textures (specular, normal, etc.) as needed
        if (m_Diffuse.Texture)
        {
            m_Diffuse.Texture->Bind();
        }
    }
}
