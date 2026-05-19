#include "Material.h"
#include "Texture.h"
#include "RHI/RHIShader.h"
#include "RHI/RHITexture.h"

#include <string>

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

    void Material::BindCompiledGraph(RHIShader& shader) const
    {
        for (size_t slotIndex = 0; slotIndex < m_GraphTextureSlots.size(); ++slotIndex)
        {
            const std::shared_ptr<Texture2D>& texture = m_GraphTextureSlots[slotIndex];
            if (!texture || texture->GetRHITexture() == nullptr)
            {
                continue;
            }

            const int textureUnit = static_cast<int>(slotIndex);
            texture->GetRHITexture()->Bind(textureUnit);
            shader.UploadUniformInt("u_Texture" + std::to_string(slotIndex), textureUnit);
        }

        for (size_t paramIndex = 0; paramIndex < m_GraphScalarParams.size(); ++paramIndex)
        {
            shader.UploadUniformFloat(
                "u_ScalarParam" + std::to_string(paramIndex),
                m_GraphScalarParams[paramIndex]);
        }
    }
}
