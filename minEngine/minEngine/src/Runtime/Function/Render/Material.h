#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/Shader.h"

namespace minEngine
{
    class Shader;

    ME_STRUCT()
    struct MaterialParameters
    {
        ME_REFLECTION_FRIEND(MaterialParameters)

        ME_PROPERTY()
        Vector4 Value{ 1.0f, 1.0f, 1.0f, 1.0f };
        ME_PROPERTY()
        std::shared_ptr<Texture2D> Texture{ nullptr };
    };

    ME_CLASS()
    class Material : public MEObject
    {
        ME_REFLECTION_FRIEND(Material)
    public:
        Material() = default;
        virtual ~Material() = default;

        virtual void BindTextures() const;

        ME_PROPERTY()
        std::shared_ptr<Shader> m_Shader;
        ME_PROPERTY()
        MaterialParameters m_Diffuse;
        ME_PROPERTY()
        MaterialParameters m_Specular;
        ME_PROPERTY()
        MaterialParameters m_Normal;
        
        bool IsTranslucent() const
        {
            // A material is considered translucent if its diffuse color has an alpha value less than 1, or if it has a diffuse texture with 4 channels (indicating it has an alpha channel)
            return m_Diffuse.Value.a < 1.0f || (m_Diffuse.Texture && m_Diffuse.Texture->GetChannels() == 4);
        }
    };
}

#include "Material.gen.h"