#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

namespace minEngine
{
    class OpenGLTexture2D : public RHITexture2D
    {
    public:
        uint32_t m_ID;

        // TODO: Add other texture creation methods (from memory, from data, etc.)
        OpenGLTexture2D(const std::string& path, uint32_t unit = 0);

        OpenGLTexture2D() = default;
        virtual ~OpenGLTexture2D() = default;
        
        virtual int GetID() const override { return m_ID; }

        virtual void Bind() override;
        virtual void Unbind() override;
    };
}