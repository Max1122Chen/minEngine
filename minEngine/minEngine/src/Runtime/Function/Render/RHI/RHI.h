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

        // --- OpenGL immediate state (window / legacy passes; not resource factories) ---

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

        // --- Modern Dynamic RHI ---

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

        virtual std::shared_ptr<RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<RHIVertexElement> elements) = 0;

        virtual void RHICmdBeginRenderPass(const RHIRenderPassInfo& info) = 0;
        virtual void RHICmdEndRenderPass() = 0;

        virtual void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState) = 0;
        virtual void RHICmdSetBindingSet(uint32_t setIndex, RHIBindingSet* bindingSet) = 0;

        virtual void RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        virtual void RHICmdSetVertexInputLayout(RHIVertexInputLayout* layout) = 0;
        virtual void RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot = 0) = 0;
        virtual void RHICmdSetIndexBuffer(RHIBuffer* indexBuffer) = 0;

        virtual void RHICmdDrawIndexed(
            uint32_t indexCount,
            uint32_t firstIndex = 0,
            int32_t vertexOffset = 0) = 0;

        virtual void RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    };
}
