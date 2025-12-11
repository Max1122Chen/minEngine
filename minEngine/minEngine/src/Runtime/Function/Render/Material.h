#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/RHIShader.h"
#include "Runtime/Function/Render/RHITexture.h"

namespace minEngine
{
    class RHITexture;

    struct MaterialParameters
    {
        Vector4 Value{ 1.0f, 1.0f, 1.0f, 1.0f };
        std::shared_ptr<RHITexture> Texture{ nullptr };
    };

    class Material
    {
    public:
        Material() = default;
        virtual ~Material() = default;


        std::shared_ptr<RHIShader> m_Shader;

        MaterialParameters m_Diffuse;
        MaterialParameters m_Specular;
        MaterialParameters m_Normal;
        
    };
}