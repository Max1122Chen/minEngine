#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHI.h"


namespace minEngine
{
    class VertexBuffer;
    class IndexBuffer;
    class RHITexture2D;
    class RHIShader;

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
        virtual std::shared_ptr<RHITexture2D> CreateTexture2D(const char* filepath, uint32_t unit) override;
        virtual std::shared_ptr<RHIShader> CreateShader(const char* vertexSource, const char* fragmentSource) override;

       
    

    private:
        std::shared_ptr<WindowSystem> m_WindowSystem;
    };
}