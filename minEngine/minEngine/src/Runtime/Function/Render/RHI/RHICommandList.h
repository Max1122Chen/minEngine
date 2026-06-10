#pragma once

#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    class MeshDrawCommand;

    struct SubmitDrawBinding
    {
        uint32_t SetIndex = 0;
        RHIBindingSet* BindingSet = nullptr;
    };

    // Concrete forwarding layer (UE FRHICommandList pattern). No per-backend subclass.
    class RHICommandList
    {
    public:
        explicit RHICommandList(RHI* rhi)
            : m_RHI(rhi)
        {
        }

        RHICommandList() = default;
        ~RHICommandList() = default;

        RHICommandList(const RHICommandList&) = default;
        RHICommandList& operator=(const RHICommandList&) = default;

        RHI* GetExecutingRHI() const { return m_RHI; }

        // Commands
        void BeginRenderPass(const RHIRenderPassInfo& info) { m_RHI->RHICmdBeginRenderPass(info); }
        void EndRenderPass() { m_RHI->RHICmdEndRenderPass(); }
        void SetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
        {
            m_RHI->RHICmdSetGraphicsPipelineState(pipelineState);
        }
        void SetBindingSet(uint32_t setIndex, RHIBindingSet* bindingSet)
        {
            m_RHI->RHICmdSetBindingSet(setIndex, bindingSet);
        }
        void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
        {
            m_RHI->RHICmdSetViewport(x, y, width, height);
        }
        void SetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot = 0)
        {
            m_RHI->RHICmdSetVertexBuffer(vertexBuffer, slot);
        }
        void SetIndexBuffer(RHIBuffer* indexBuffer) { m_RHI->RHICmdSetIndexBuffer(indexBuffer); }
        void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0)
        {
            m_RHI->RHICmdDrawIndexed(indexCount, firstIndex, vertexOffset);
        }
        void Draw(uint32_t vertexCount, uint32_t firstVertex = 0)
        {
            m_RHI->RHICmdDraw(vertexCount, firstVertex);
        }

        /** Fixed four-step draw path: PSO → binding sets → vertex/index buffers → Draw. */
        void SubmitDraw(
            RHIGraphicsPipelineState* pipelineState,
            const SubmitDrawBinding* bindingSets,
            uint32_t bindingSetCount,
            RHIBuffer* vertexBuffer,
            RHIBuffer* indexBuffer);

        void SubmitDrawMesh(
            RHIGraphicsPipelineState* pipelineState,
            const SubmitDrawBinding* bindingSets,
            uint32_t bindingSetCount,
            const MeshDrawCommand& drawCommand);

        // Resource creation
        RHITextureRef CreateTexture2D(const RHITextureCreateDesc& desc, const void* initialData = nullptr)
        {
            return m_RHI->RHICreateTexture2D(desc, initialData);
        }
        RHIBufferRef CreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData = nullptr)
        {
            return m_RHI->RHICreateBuffer(desc, initialData);
        }
        RHIShaderRef CreateShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog = nullptr)
        {
            return m_RHI->RHICreateShader(vertexSource, fragmentSource, outCompileLog);
        }
        RHIGraphicsPipelineStateRef CreateGraphicsPipelineState(const RHIGraphicsPSODesc& desc)
        {
            return m_RHI->RHICreateGraphicsPipelineState(desc);
        }
        RHIBindingLayoutRef CreateBindingLayout(const std::vector<RHIBindingLayoutEntry>& entries)
        {
            return m_RHI->RHICreateBindingLayout(entries);
        }
        RHIBindingSetRef CreateBindingSet(
            RHIBindingLayout* layout,
            const std::vector<RHIBindingResource>& resources)
        {
            return m_RHI->RHICreateBindingSet(layout, resources);
        }
        std::shared_ptr<RHIVertexInputLayout> CreateVertexInputLayout(std::initializer_list<RHIVertexElement> elements)
        {
            return m_RHI->RHICreateVertexInputLayout(elements);
        }

    private:
        RHI* m_RHI = nullptr;
    };
}
