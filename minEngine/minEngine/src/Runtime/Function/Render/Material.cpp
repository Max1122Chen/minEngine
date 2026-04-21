#include "Material.h"
#include "Texture.h"
#include "RHI/RHITexture.h"

namespace minEngine
{
    void Material::BindTextures() const
    {
        // TODO: bind other textures (specular, normal, etc.) as needed
        if (m_Diffuse.Texture)
        {
            m_Diffuse.Texture->GetRHITexture()->Bind(1); // Bind diffuse texture to texture unit 1
        }
    }
}
