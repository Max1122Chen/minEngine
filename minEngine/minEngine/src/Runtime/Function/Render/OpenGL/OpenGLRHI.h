#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "OpenGLRHIModern.h"


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
        virtual std::shared_ptr<VertexDefinition> CreateVertexDefinition(std::initializer_list<RHIVertexElement> elements) override;
        virtual std::shared_ptr<FrameBuffer> CreateFrameBuffer(uint32_t width, uint32_t height) override;
        virtual std::shared_ptr<UniformBuffer> CreateUniformBuffer(uint32_t size, uint32_t bindingPoint = 0) override;
        virtual std::shared_ptr<RHITexture2D> CreateRHITexture2D(const unsigned char* data, RHITextureDesc desc) override;
        virtual std::shared_ptr<RHITexture2D> CreateRHITexture2DFloat(const float* data, RHITextureDesc desc) override;
        virtual std::shared_ptr<RHITextureCube> CreateRHITextureCube(
            const std::vector<unsigned char*>& faceData,
            RHITextureDesc desc,
            bool generateMipmaps = false) override;
        virtual std::shared_ptr<RHITexture2DArray> CreateRHITexture2DArray(const unsigned char* data, RHITextureDesc desc) override;
        virtual std::shared_ptr<RHIShaderLegacy> CreateRHIShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog = nullptr) override;

        // Modern RHI (S3 stubs — S4 implements)
        virtual std::shared_ptr<RHITexture> RHICreateTexture2D(
            const RHITextureCreateDesc& desc,
            const void* initialData) override;
        virtual std::shared_ptr<RHIBuffer> RHICreateBuffer(
            const RHIBufferCreateDesc& desc,
            const void* initialData) override;
        virtual std::shared_ptr<RHIShader> RHICreateShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog) override;
        virtual std::shared_ptr<RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const RHIGraphicsPSODesc& desc) override;
        virtual std::shared_ptr<RHIBindingLayout> RHICreateBindingLayout(
            const std::vector<RHIBindingLayoutEntry>& entries) override;
        virtual std::shared_ptr<RHIBindingSet> RHICreateBindingSet(
            RHIBindingLayout* layout,
            const std::vector<RHIBindingResource>& resources) override;

        virtual void RHICmdBeginRenderPass(const RHIRenderPassInfo& info) override;
        virtual void RHICmdEndRenderPass() override;
        virtual void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState) override;
        virtual void RHICmdSetBindingSet(uint32_t setIndex, RHIBindingSet* bindingSet) override;
        virtual std::shared_ptr<RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<RHIVertexElement> elements) override;

        virtual void RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        virtual void RHICmdSetVertexInputLayout(RHIVertexInputLayout* layout) override;
        virtual void RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot) override;
        virtual void RHICmdSetIndexBuffer(RHIBuffer* indexBuffer) override;
        virtual void RHICmdDrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
        virtual void RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex) override;

    private:
        void ApplyGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState);
        void DestroyTransientFramebuffer();

        WindowSystem* m_WindowSystem = nullptr;

        GLuint m_TransientFramebuffer = 0;
        bool m_OwnsTransientFramebuffer = false;
        RHIGraphicsPipelineState* m_BoundPipeline = nullptr;
        OpenGLRHIVertexInputLayout* m_BoundVertexLayout = nullptr;
        OpenGLRHIBuffer* m_BoundVertexBuffer = nullptr;
        OpenGLRHIBuffer* m_BoundIndexBuffer = nullptr;
    };
}