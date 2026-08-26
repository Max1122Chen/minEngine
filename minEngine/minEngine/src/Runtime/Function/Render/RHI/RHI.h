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
    struct RHIShaderCreateDesc;

    class RHIVertexInputLayout;

    class RHIGraphicsPipelineState;
    struct RHIGraphicsPSODesc;

    class RHIShaderBindingSetLayout;
    struct RHIShaderBindingSetLayoutEntry;

    class RHIPipelineLayout;

    class RHIShaderBindingSet;
    struct RHIShaderBinding;

    class RHIShaderResourceView;
    struct RHITextureSRVDesc;

    class RHIRenderPassInfo;

    struct RHITextureTransitionInfo;

    class RHI
    {
    public:
        RHI() = default;
        virtual ~RHI() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual std::shared_ptr<RHITexture> RHICreateTexture2D(
            const RHITextureCreateDesc& desc,
            const void* initialData = nullptr) = 0;

        virtual std::shared_ptr<RHIShaderResourceView> RHICreateShaderResourceView(
            const RHITextureSRVDesc& desc) = 0;

        virtual std::shared_ptr<RHIBuffer> RHICreateBuffer(
            const RHIBufferCreateDesc& desc,
            const void* initialData = nullptr) = 0;

        virtual std::shared_ptr<RHIShader> RHICreateShader(
            const RHIShaderCreateDesc& desc,
            std::string* outCompileLog = nullptr) = 0;

        /** Legacy GLSL source path (Material / unmigrated passes). Prefer bytecode overload. */
        virtual std::shared_ptr<RHIShader> RHICreateShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog = nullptr) = 0;

        virtual std::shared_ptr<RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const RHIGraphicsPSODesc& desc) = 0;

        virtual std::shared_ptr<RHIShaderBindingSetLayout> RHICreateShaderBindingSetLayout(
            const std::vector<RHIShaderBindingSetLayoutEntry>& entries) = 0;

        virtual std::shared_ptr<RHIPipelineLayout> RHICreatePipelineLayout(
            const std::vector<RHIShaderBindingSetLayout*>& setLayouts) = 0;

        virtual std::shared_ptr<RHIShaderBindingSet> RHICreateShaderBindingSet(
            RHIShaderBindingSetLayout* layout,
            const std::vector<RHIShaderBinding>& resources) = 0;

        virtual std::shared_ptr<RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<RHIVertexElement> elements) = 0;

        virtual void RHICmdBeginRenderPass(const RHIRenderPassInfo& info) = 0;
        virtual void RHICmdEndRenderPass() = 0;

        virtual void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState) = 0;
        virtual void RHICmdSetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet) = 0;
        virtual void RHICmdTransition(const RHITextureTransitionInfo& transition) = 0;

        virtual void RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        virtual void RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot = 0) = 0;
        virtual void RHICmdSetIndexBuffer(RHIBuffer* indexBuffer) = 0;

        virtual void RHICmdDrawIndexed(
            uint32_t indexCount,
            uint32_t firstIndex = 0,
            int32_t vertexOffset = 0) = 0;

        virtual void RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;

        /** Generate mip chain from mip 0 (backend-owned). Prefer after filling base level. */
        virtual void RHICmdGenerateMips(RHITexture* texture) = 0;

        /** Backbuffer clear color / clear (swapchain surface). */
        virtual void RHISetBackbufferClearColor(const Vector3& color) = 0;
        virtual void RHIClearBackbuffer() = 0;

        /**
         * Present the backbuffer / swapchain.
         * OpenGL: glfwSwapBuffers. Vulkan: submit + present (semaphores/fences internal).
         */
        virtual void RHIPresent() = 0;

        /**
         * Offline / load-time GPU work outside the swapchain frame (EnvMap bake, etc.).
         * OpenGL: no-op. Vulkan: one-shot command buffer + queue wait.
         */
        virtual void RHIBeginImmediateCommands() {}
        virtual void RHIEndImmediateCommands() {}
    };
}
