#include "RenderPassBase.h"

#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPipeline.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

namespace minEngine
{
    void RenderPassBase::DrawMeshCommand(RHICommandList& cmdList, const MeshDrawCommand& drawCommand)
    {
        if (drawCommand.m_VertexInputLayout)
        {
            cmdList.SetVertexInputLayout(drawCommand.m_VertexInputLayout);
        }
        if (drawCommand.m_VertexBuffer)
        {
            cmdList.SetVertexBuffer(drawCommand.m_VertexBuffer);
        }

        if (drawCommand.m_IndexBuffer)
        {
            cmdList.SetIndexBuffer(drawCommand.m_IndexBuffer);
            cmdList.DrawIndexed(drawCommand.m_IndexBuffer->GetDesc().ElementCount, 0, 0);
        }
        else if (drawCommand.m_VertexBuffer)
        {
            cmdList.Draw(drawCommand.m_VertexBuffer->GetDesc().ElementCount, 0);
        }
    }

    void RenderPassBase::BindSceneDrawResources(
        RHICommandList& cmdList,
        const RenderPipeline& pipeline,
        const MeshPassSceneBinding& binding)
    {
        const EngineSceneBindingSets& sceneBindings = pipeline.GetSceneBindings();
        RHIBuffer* perObjectBuffer = pipeline.GetPerObjectUniformBuffer();
        sceneBindings.UpdatePerObjectModel(perObjectBuffer, binding.DrawCommand.m_ModelMatrix);

        cmdList.SetBindingSet(EngineShaderBindings::kSetSceneObject, sceneBindings.GetSceneSet0());

        if (!binding.bBindLighting)
        {
            return;
        }

        cmdList.SetBindingSet(EngineShaderBindings::kSetShadowIBL, sceneBindings.GetSceneSet1());
    }
}
