#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    class RHIShader;
    class Texture2D;

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
        
    };
}