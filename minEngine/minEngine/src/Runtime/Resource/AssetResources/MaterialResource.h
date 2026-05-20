#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/Texture.h"

#include <memory>
#include <string>

namespace minEngine
{
    // Legacy on-disk material fields (.memtl); runtime Material uses graph compilation instead.
    ME_STRUCT()
    struct MaterialResourceParameters
    {
        ME_GENERATED_BODY(MaterialResourceParameters)

        ME_PROPERTY()
        Vector4 Value{ 1.0f, 1.0f, 1.0f, 1.0f };
        ME_PROPERTY()
        std::shared_ptr<Texture2D> Texture{ nullptr };
    };

    ME_CLASS()
    class MaterialResource
    {
        ME_GENERATED_BODY(MaterialResource)
    public:
        MaterialResource() = default;
        ~MaterialResource() = default;

        ME_PROPERTY()
        std::string m_Name;

        ME_PROPERTY()
        std::string m_ShaderPath;

        ME_PROPERTY()
        MaterialResourceParameters m_Diffuse;
    };
}

#include "MaterialResource.gen.h"
