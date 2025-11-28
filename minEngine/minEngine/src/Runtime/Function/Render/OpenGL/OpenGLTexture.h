#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHITexture.h"

namespace minEngine
{
    class OpenGLTexture : public RHITexture
    {
    public:
        uint32_t m_ID;

        // TODO: Add other texture creation methods (from memory, from data, etc.)
        OpenGLTexture(const char* path, uint32_t unit = 0);

        OpenGLTexture() = default;
        virtual ~OpenGLTexture() = default;
        
        virtual void Bind() override;
        virtual void Unbind() override;
    };
}