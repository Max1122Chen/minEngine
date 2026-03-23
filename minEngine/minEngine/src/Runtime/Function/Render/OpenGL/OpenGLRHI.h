#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHI.h"


namespace minEngine
{

    class WindowSystem;

    class OpenGLRHI : public RHI
    {
    public:
        friend class RenderSystem;

        OpenGLRHI() = default;
        virtual ~OpenGLRHI() = default;

        virtual void Initialize() override;
        virtual void Shutdown() override;

        virtual void EnableDepthTest() override;
        virtual void DisableDepthTest() override;
        virtual void EnableCullFace() override;
        virtual void DisableCullFace() override;

        virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size, uint32_t numVertices) override;
        virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t numIndices) override;
        virtual std::shared_ptr<VertexDefinition> CreateVertexDefinition(std::initializer_list<VertexElement> elements) override;
        virtual std::shared_ptr<FrameBuffer> CreateFrameBuffer(uint32_t width, uint32_t height, bool bHasDepth) override;
        virtual std::shared_ptr<RHIShader> CreateShader(const char* vertexSource, const char* fragmentSource) override;

       
    

    private:
        std::shared_ptr<WindowSystem> m_WindowSystem;
    };
}