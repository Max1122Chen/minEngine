#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI.h"


namespace minEngine
{
    class VertexBuffer;
    class IndexBuffer;
    class RHITexture;
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

        virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size) override;
        virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t count) override;
        virtual std::shared_ptr<RHITexture> CreateTexture(const char* filepath, uint32_t unit) override;
        virtual std::shared_ptr<RHIShader> CreateShader(const char* vertexSource, const char* fragmentSource) override;

       
    

    private:
        std::shared_ptr<WindowSystem> m_WindowSystem;
    };
}