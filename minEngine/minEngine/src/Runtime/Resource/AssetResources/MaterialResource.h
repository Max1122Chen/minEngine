#pragma once
#include "Core.h"
#include "Runtime/Function/Render/Material.h"

namespace minEngine
{
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
        MaterialParameters m_Diffuse;
        
    private:
       
    };
}

#include "MaterialResource.gen.h"