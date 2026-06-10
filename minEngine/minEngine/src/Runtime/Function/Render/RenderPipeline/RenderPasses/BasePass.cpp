#include "BasePass.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPipeline.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

namespace minEngine
{
    void BasePass::Execute()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Execute(cmdList);
    }

    void BasePass::Execute(RHICommandList& cmdList)
    {
        Render(cmdList);
    }

    void BasePass::Render(RHICommandList& cmdList)
    {
        if (!pipeline)
        {
            return;
        }

        for (MeshDrawCommand& drawCommand : m_DrawCommands)
        {
            Material* material = drawCommand.m_Material;
            if (!material || !drawCommand.m_VertexInputLayout || !drawCommand.m_VertexBuffer)
            {
                continue;
            }

            if (!material->IsCompiledForDraw() || !material->GetPipelineState())
            {
                continue;
            }

            cmdList.SetGraphicsPipelineState(material->GetPipelineState());

            const bool bindSceneLighting = material->m_ShadingModel == MaterialShadingModel::BlinnPhong
                || material->m_ShadingModel == MaterialShadingModel::PBR;

            const MeshPassSceneBinding sceneBinding{
                drawCommand,
                bindSceneLighting,
                material->m_ShadingModel == MaterialShadingModel::PBR,
                &m_DirectionalShadowHandle,
                &m_SpotShadowHandles,
                &m_PointShadowHandles,
            };
            BindSceneDrawResources(cmdList, *pipeline, sceneBinding);
            material->BindForDraw(cmdList);
            DrawMeshCommand(cmdList, drawCommand);
        }
    }
}
