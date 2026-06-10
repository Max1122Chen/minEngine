#include "RHICommandList.h"

#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

namespace minEngine
{
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

        if (bindingSets)
        {
            for (uint32_t bindingIndex = 0; bindingIndex < bindingSetCount; ++bindingIndex)
            {
                const SubmitDrawBinding& binding = bindingSets[bindingIndex];
                if (binding.BindingSet)
                {
                    SetBindingSet(binding.SetIndex, binding.BindingSet);
                }
            }
        }

        if (indexBuffer)
        {
            if (vertexBuffer)
            {
                SetVertexBuffer(vertexBuffer);
            }
            SetIndexBuffer(indexBuffer);
            DrawIndexed(indexBuffer->GetDesc().ElementCount, 0, 0);
            return;
        }

        if (!vertexBuffer)
        {
            return;
        }

        SetVertexBuffer(vertexBuffer);
        Draw(vertexBuffer->GetDesc().ElementCount, 0);
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
}
