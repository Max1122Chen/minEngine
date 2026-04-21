#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

namespace minEngine
{
    ME_CLASS()
    class Shader : public MEObject
    {
        ME_REFLECTION_FRIEND(Shader)
        friend class AssetManager;
    public:
        Shader() = default;
        ~Shader() = default;

        std::shared_ptr<RHIShader> GetRHIShader() const { return m_RHIShader; }

    private:
        ME_PROPERTY()
        std::shared_ptr<RHIShader> m_RHIShader;
    };
}

#include "Shader.gen.h"