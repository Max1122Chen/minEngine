#pragma once
#include "Core.h"

namespace minEngine
{
    class VertexBuffer;
    class IndexBuffer;
    class RHITexture;
    class RHIShader;

    class RHI
    {
    public:
        RHI() = default;
        virtual ~RHI() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual void EnableDepthTest() = 0;
        virtual void DisableDepthTest() = 0;

        virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size) = 0;
        virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t count) = 0;
        virtual std::shared_ptr<RHITexture> CreateTexture(const char* filepath, uint32_t unit) = 0;
        virtual std::shared_ptr<RHIShader> CreateShader(const char* vertexSource, const char* fragmentSource) = 0;
    };
}