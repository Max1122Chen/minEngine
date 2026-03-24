#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    class RHIShader;

    struct MaterialParameters
    {
        Vector4 Value{ 1.0f, 1.0f, 1.0f, 1.0f };
        std::shared_ptr<Texture2D> Texture{ nullptr };
    };

    class Material
    {
    public:
        Material() = default;
        virtual ~Material() = default;

        virtual void BindTextures() const;

        std::shared_ptr<RHIShader> m_Shader;

        MaterialParameters m_Diffuse;
        MaterialParameters m_Specular;
        MaterialParameters m_Normal;
        
        bool IsTranslucent() const
        {
            // A material is considered translucent if its diffuse color has an alpha value less than 1, or if it has a diffuse texture with 4 channels (indicating it has an alpha channel)
            return m_Diffuse.Value.a < 1.0f || (m_Diffuse.Texture && m_Diffuse.Texture->GetChannels() == 4);
        }
    };
}