#pragma once
#include "Core.h"

namespace minEngine
{
    struct VertexElement;
    class VertexDefinition;
    class VertexBuffer;
    class IndexBuffer;
    class FrameBuffer;
    class RHITexture2D;
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
        virtual void EnableCullFace() = 0;
        virtual void DisableCullFace() = 0;

        virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size, uint32_t numVertices) = 0;
        virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t numIndices) = 0;
        virtual std::shared_ptr<VertexDefinition> CreateVertexDefinition(std::initializer_list<VertexElement> elements) = 0;
        virtual std::shared_ptr<FrameBuffer> CreateFrameBuffer(uint32_t width, uint32_t height, bool bHasDepth) = 0;
        virtual std::shared_ptr<RHIShader> CreateShader(const char* vertexSource, const char* fragmentSource) = 0;
    };
}