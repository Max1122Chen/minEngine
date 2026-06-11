#include "RHICommandList.h"

#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

namespace minEngine
{
    namespace
    {
        void BindPacketDescriptorSets(RHICommandList& cmdList, const MeshDrawPacket& packet)
        {
            RHIGraphicsPipelineState* pipelineState = packet.PipelineState.get();
            RHIPipelineLayout* pipelineLayout = nullptr;
            if (pipelineState)
            {
                if (auto* fallback = dynamic_cast<RHIGraphicsPSOStateFallback*>(pipelineState))
                {
                    pipelineLayout = fallback->GetDesc().PipelineLayout;
                }
            }

            if (pipelineLayout)
            {
                const uint32_t setCount = pipelineLayout->GetSetLayoutCount();
                for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
                {
                    if (packet.BindingSets[setIndex])
                    {
                        cmdList.SetBindingSet(setIndex, packet.BindingSets[setIndex]);
                    }
                }
                return;
            }

            for (uint32_t setIndex = 0; setIndex < kMaxPipelineDescriptorSets; ++setIndex)
            {
                if (packet.BindingSets[setIndex])
                {
                    cmdList.SetBindingSet(setIndex, packet.BindingSets[setIndex]);
                }
            }
        }

        void SubmitDrawBuffers(RHICommandList& cmdList, const MeshDrawPacket& packet)
        {
            if (packet.IndexBuffer)
            {
                if (packet.VertexBuffer)
                {
                    cmdList.SetVertexBuffer(packet.VertexBuffer);
                }
                cmdList.SetIndexBuffer(packet.IndexBuffer);

                const uint32_t indexCount = packet.IndexCount != 0
                    ? packet.IndexCount
                    : packet.IndexBuffer->GetDesc().ElementCount;
                cmdList.DrawIndexed(indexCount, packet.FirstIndex, packet.VertexOffset);
                return;
            }

            if (!packet.VertexBuffer)
            {
                return;
            }

            cmdList.SetVertexBuffer(packet.VertexBuffer);
            const uint32_t vertexCount = packet.VertexCount != 0
                ? packet.VertexCount
                : packet.VertexBuffer->GetDesc().ElementCount;
            cmdList.Draw(vertexCount, packet.FirstVertex);
        }

        void BindLegacyDescriptorSets(
            RHICommandList& cmdList,
            const SubmitDrawBinding* bindingSets,
            uint32_t bindingSetCount)
        {
            if (!bindingSets)
            {
                return;
            }

            for (uint32_t bindingIndex = 0; bindingIndex < bindingSetCount; ++bindingIndex)
            {
                const SubmitDrawBinding& binding = bindingSets[bindingIndex];
                if (binding.BindingSet)
                {
                    cmdList.SetBindingSet(binding.SetIndex, binding.BindingSet);
                }
            }
        }

    }

    void RHICommandList::SubmitDraw(
        RHIGraphicsPipelineState* pipelineState,
        const SubmitDrawBinding* bindingSets,
        uint32_t bindingSetCount,
        RHIBuffer* vertexBuffer,
        RHIBuffer* indexBuffer)
    {
        if (!pipelineState)
        {
            return;
        }

        SetGraphicsPipelineState(pipelineState);
        BindLegacyDescriptorSets(*this, bindingSets, bindingSetCount);

        MeshDrawPacket bufferPacket;
        bufferPacket.VertexBuffer = vertexBuffer;
        bufferPacket.IndexBuffer = indexBuffer;
        SubmitDrawBuffers(*this, bufferPacket);
    }

    void RHICommandList::SubmitDrawMesh(
        RHIGraphicsPipelineState* pipelineState,
        const SubmitDrawBinding* bindingSets,
        uint32_t bindingSetCount,
        const MeshDrawCommand& drawCommand)
    {
        SubmitDraw(
            pipelineState,
            bindingSets,
            bindingSetCount,
            drawCommand.m_VertexBuffer,
            drawCommand.m_IndexBuffer);
    }

    void RHICommandList::SubmitMeshDrawPacket(const MeshDrawPacket& packet)
    {
        if (!packet.PipelineState)
        {
            return;
        }

        SetGraphicsPipelineState(packet.PipelineState.get());
        BindPacketDescriptorSets(*this, packet);
        SubmitDrawBuffers(*this, packet);
    }
}
