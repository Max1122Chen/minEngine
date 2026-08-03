#include "RHICommandList.h"

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
                const uint32_t setCount = pipelineLayout->GetShaderBindingSetLayoutCount();
                for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
                {
                    if (packet.ShaderBindingSets[setIndex])
                    {
                        cmdList.SetShaderBindingSet(setIndex, packet.ShaderBindingSets[setIndex]);
                    }
                }
                return;
            }

            for (uint32_t setIndex = 0; setIndex < kMaxShaderBindingSets; ++setIndex)
            {
                if (packet.ShaderBindingSets[setIndex])
                {
                    cmdList.SetShaderBindingSet(setIndex, packet.ShaderBindingSets[setIndex]);
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
