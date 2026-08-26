#pragma once

#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIPipelineLayout.h"
#include "Render/RHI/RHIResourceTransition.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    class MeshDrawCommand;

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
        void SetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet)
        {
            m_RHI->RHICmdSetShaderBindingSet(setIndex, bindingSet);
        }
        void Transition(const RHITextureTransitionInfo& transition)
        {
            m_RHI->RHICmdTransition(transition);
        }
        void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool flipY = true)
        {
            m_RHI->RHICmdSetViewport(x, y, width, height, flipY);
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
        void GenerateMips(RHITexture* texture) { m_RHI->RHICmdGenerateMips(texture); }

        /** Sole draw submit path: PSO ? all binding sets ? VB/IB ? Draw. */
        void SubmitMeshDrawPacket(const MeshDrawPacket& packet);

        // Resource creation
        RHITextureRef CreateTexture2D(const RHITextureCreateDesc& desc, const void* initialData = nullptr)
        {
            return m_RHI->RHICreateTexture2D(desc, initialData);
        }
        RHIShaderResourceViewRef CreateShaderResourceView(const RHITextureSRVDesc& desc)
        {
            return m_RHI->RHICreateShaderResourceView(desc);
        }
        RHIBufferRef CreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData = nullptr)
        {
            return m_RHI->RHICreateBuffer(desc, initialData);
        }
        RHIShaderRef CreateShader(
            const RHIShaderCreateDesc& desc,
            std::string* outCompileLog = nullptr)
        {
            return m_RHI->RHICreateShader(desc, outCompileLog);
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
        RHIShaderBindingSetLayoutRef CreateShaderBindingSetLayout(const std::vector<RHIShaderBindingSetLayoutEntry>& entries)
        {
            return m_RHI->RHICreateShaderBindingSetLayout(entries);
        }
        RHIPipelineLayoutRef CreatePipelineLayout(const std::vector<RHIShaderBindingSetLayout*>& setLayouts)
        {
            return m_RHI->RHICreatePipelineLayout(setLayouts);
        }
        RHIShaderBindingSetRef CreateShaderBindingSet(
            RHIShaderBindingSetLayout* layout,
            const std::vector<RHIShaderBinding>& resources)
        {
            return m_RHI->RHICreateShaderBindingSet(layout, resources);
        }
        std::shared_ptr<RHIVertexInputLayout> CreateVertexInputLayout(std::initializer_list<RHIVertexElement> elements)
        {
            return m_RHI->RHICreateVertexInputLayout(elements);
        }

    private:
        RHI* m_RHI = nullptr;
    };
}
