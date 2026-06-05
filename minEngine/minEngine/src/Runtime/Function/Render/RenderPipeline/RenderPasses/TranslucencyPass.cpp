#include "TranslucencyPass.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Render/Shader.h"

namespace minEngine
{
    void TranslucencyPass::Execute()
    {
        SortDrawCommands();
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Execute(cmdList);
    }

    void TranslucencyPass::Execute(RHICommandList& cmdList)
    {
        Render(cmdList);
    }

    void TranslucencyPass::Render(RHICommandList& cmdList)
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }

        rhi->EnableBlend();
        rhi->EnableDepthTest();
        rhi->SetDepthMask(false);

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

        rhi->SetDepthMask(true);
        rhi->DisableBlend();
    }

    void TranslucencyPass::SortDrawCommands()
    {
        RenderCamera* mainCamera = m_SortCamera;
        if (!mainCamera)
        {
            return;
        }

        const Vector3 cameraPos = mainCamera->m_Position;

        std::sort(m_DrawCommands.begin(), m_DrawCommands.end(), [cameraPos](const MeshDrawCommand& a, const MeshDrawCommand& b)
            {
                const Vector3 deltaA = cameraPos - glm::vec3(a.m_ModelMatrix[3]);
                const Vector3 deltaB = cameraPos - glm::vec3(b.m_ModelMatrix[3]);

                const float distanceA = glm::dot(deltaA, deltaA);
                const float distanceB = glm::dot(deltaB, deltaB);
                return distanceA > distanceB;
            });
    }
}
