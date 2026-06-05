#include "BasePass.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Render/Shader.h"

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
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        rhi->EnableDepthTest();
        rhi->SetDepthMask(true);

        for (MeshDrawCommand& drawCommand : m_DrawCommands)
        {
            Material* material = drawCommand.m_Material;
            if (!material || !drawCommand.m_VertexInputLayout || !drawCommand.m_VertexBuffer)
            {
                continue;
            }

            if (!material->IsCompiledForDraw())
            {
                continue;
            }

            RHIShaderLegacy* shader = material->GetShader()->GetRHIShader().get();
            shader->Use();

            const bool bindSceneLighting = material->m_ShadingModel == MaterialShadingModel::BlinnPhong
                || material->m_ShadingModel == MaterialShadingModel::PBR;
            const bool bindPBRIBL = material->m_ShadingModel == MaterialShadingModel::PBR;

            const MeshPassSceneBinding sceneBinding{
                drawCommand,
                bindSceneLighting,
                bindPBRIBL,
                &m_DirectionalShadowHandle,
                &m_SpotShadowHandles,
                &m_PointShadowHandles,
                (bindPBRIBL && pipeline != nullptr) ? &pipeline->GetIBLEnvironment() : nullptr,
            };
            BindSceneDrawResources(*shader, sceneBinding);
            material->BindForDraw(*shader);
            if (bindPBRIBL && sceneBinding.IBLEnvironment != nullptr)
            {
                sceneBinding.IBLEnvironment->BindForPBRDraw(*shader);
            }
            DrawMeshCommand(cmdList, drawCommand);
        }
    }
}
