#pragma once
#include "Core.h"
#include "Math/Math.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    struct RHIVertexElement;
    class VertexDefinition;
    class VertexBuffer;
    class IndexBuffer;
    class FrameBuffer;
    class UniformBuffer;
    class RHITexture2D;
    class RHITextureCube;
    class RHITexture2DArray;
    struct RHITextureDesc;
    class RHIShaderLegacy;

    class RHITexture;
    struct RHITextureCreateDesc;

    class RHIBuffer;
    struct RHIBufferCreateDesc;

    class RHIShader;

    class RHIVertexInputLayout;

    class RHIGraphicsPipelineState;
    struct RHIGraphicsPSODesc;

    class RHIBindingLayout;
    struct RHIBindingLayoutEntry;

    class RHIBindingSet;
    struct RHIBindingResource;

    class RHIRenderPassInfo;

    class RHI
    {
    public:
        RHI() = default;
        virtual ~RHI() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        // --- Legacy (OpenGL immediate-style; removed in S5+) ---

        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

        virtual void SetClearColor(Vector4 clearColor) = 0;
        virtual void Clear() = 0;

        virtual void SetDrawBuffer(uint32_t index) = 0;
        virtual void SetReadBuffer(uint32_t index) = 0;

        virtual void EnableDepthTest() = 0;
        virtual void DisableDepthTest() = 0;
        virtual void SetDepthMask(bool bEnable) = 0;

        virtual void EnableStencilTest() = 0;
        virtual void DisableStencilTest() = 0;
        virtual void SetStencilMask(uint32_t mask) = 0;

        virtual void EnableBlend() = 0;
        virtual void DisableBlend() = 0;

        virtual void EnableCullFace() = 0;
        virtual void DisableCullFace() = 0;

        virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size, uint32_t numVertices) = 0;
        virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t numIndices) = 0;
        virtual std::shared_ptr<VertexDefinition> CreateVertexDefinition(std::initializer_list<RHIVertexElement> elements) = 0;
        virtual std::shared_ptr<FrameBuffer> CreateFrameBuffer(uint32_t width, uint32_t height) = 0;
        virtual std::shared_ptr<UniformBuffer> CreateUniformBuffer(uint32_t size, uint32_t bindingPoint = 0) = 0;
        virtual std::shared_ptr<RHITexture2D> CreateRHITexture2D(const unsigned char* data, RHITextureDesc desc) = 0;
        virtual std::shared_ptr<RHITexture2D> CreateRHITexture2DFloat(const float* data, RHITextureDesc desc) = 0;
        virtual std::shared_ptr<RHITextureCube> CreateRHITextureCube(
            const std::vector<unsigned char*>& faceData,
            RHITextureDesc desc,
            bool generateMipmaps = false) = 0;
        virtual std::shared_ptr<RHITexture2DArray> CreateRHITexture2DArray(const unsigned char* data, RHITextureDesc desc) = 0;
        virtual std::shared_ptr<RHIShaderLegacy> CreateRHIShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog = nullptr) = 0;

        // --- Modern Dynamic RHI (RND-F02 S3; implemented in S4) ---

        virtual std::shared_ptr<RHITexture> RHICreateTexture2D(
            const RHITextureCreateDesc& desc,
            const void* initialData = nullptr) = 0;

        virtual std::shared_ptr<RHIBuffer> RHICreateBuffer(
            const RHIBufferCreateDesc& desc,
            const void* initialData = nullptr) = 0;

        virtual std::shared_ptr<RHIShader> RHICreateShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog = nullptr) = 0;

        virtual std::shared_ptr<RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const RHIGraphicsPSODesc& desc) = 0;

        virtual std::shared_ptr<RHIBindingLayout> RHICreateBindingLayout(
            const std::vector<RHIBindingLayoutEntry>& entries) = 0;

        virtual std::shared_ptr<RHIBindingSet> RHICreateBindingSet(
            RHIBindingLayout* layout,
            const std::vector<RHIBindingResource>& resources) = 0;

        virtual void RHICmdBeginRenderPass(const RHIRenderPassInfo& info) = 0;
        virtual void RHICmdEndRenderPass() = 0;

        virtual void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState) = 0;
        virtual void RHICmdSetBindingSet(uint32_t setIndex, RHIBindingSet* bindingSet) = 0;

        virtual void RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        virtual void RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot = 0) = 0;
        virtual void RHICmdSetIndexBuffer(RHIBuffer* indexBuffer) = 0;

        virtual void RHICmdDrawIndexed(
            uint32_t indexCount,
            uint32_t firstIndex = 0,
            int32_t vertexOffset = 0) = 0;

        virtual void RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    };
}
