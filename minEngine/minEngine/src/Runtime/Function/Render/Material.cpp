#include "Material.h"
#include "Texture.h"
#include "RHI/RHITexture.h"

namespace minEngine
{
    void Material::BindTextures() const
    {
        // Keep material samplers on deterministic texture units.
        if (m_Diffuse.Texture)
        {
            m_Diffuse.Texture->GetRHITexture()->Bind(0); // Bind diffuse texture to texture unit 0
        }
    }
}
