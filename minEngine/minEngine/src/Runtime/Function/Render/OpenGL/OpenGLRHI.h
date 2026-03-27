#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHI.h"


namespace minEngine
{

    class WindowSystem;
    class FrameBuffer;

    class OpenGLRHI : public RHI
    {
    public:
        friend class RenderSystem;

        OpenGLRHI() = default;
        virtual ~OpenGLRHI() = default;

        virtual void Initialize() override;
        virtual void Shutdown() override;

        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

        virtual void SetClearColor(Vector4 clearColor) override;
        virtual void Clear() override;

        virtual void SetDrawBuffer(uint32_t index) override;
        virtual void SetReadBuffer(uint32_t index) override;

        virtual void EnableDepthTest() override;
        virtual void DisableDepthTest() override;
        virtual void SetDepthMask(bool bEnable) override;

        virtual void EnableStencilTest() override;
        virtual void DisableStencilTest() override;
        virtual void SetStencilMask(uint32_t mask) override;

        virtual void EnableBlend() override;
        virtual void DisableBlend() override;

        virtual void EnableCullFace() override;
        virtual void DisableCullFace() override;

        virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size, uint32_t numVertices) override;
        virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t numIndices) override;
        virtual std::shared_ptr<VertexDefinition> CreateVertexDefinition(std::initializer_list<VertexElement> elements) override;
        virtual std::shared_ptr<FrameBuffer> CreateFrameBuffer(uint32_t width, uint32_t height) override;
        virtual std::shared_ptr<UniformBuffer> CreateUniformBuffer(uint32_t size, uint32_t bindingPoint = 0) override;
        virtual std::shared_ptr<RHITexture2D> CreateRHITexture2D(const unsigned char* data, RHITextureDesc desc, int unit = 0) override;
        virtual std::shared_ptr<RHITextureCube> CreateRHITextureCube(const std::vector<unsigned char*> faceData, RHITextureDesc desc, int unit = 0) override;
        virtual std::shared_ptr<RHITexture2DArray> CreateRHITexture2DArray(const unsigned char* data, RHITextureDesc desc, int unit = 0) override;
        virtual std::shared_ptr<RHIShader> CreateShader(const char* vertexSource, const char* fragmentSource) override;
    

    private:
        std::shared_ptr<WindowSystem> m_WindowSystem;
    };
}