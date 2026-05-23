#include "BasePass.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Render/Shader.h"

namespace minEngine
{
    void BasePass::Execute()
    {
        Render();
    }

    void BasePass::Render()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        rhi->EnableDepthTest();

        for (MeshDrawCommand& drawCommand : m_DrawCommands)
        {
            Material* material = drawCommand.m_Material;
            if (!material || !drawCommand.m_VertexDefinition || !drawCommand.m_VertexBuffer)
            {
                continue;
            }

            if (!material->IsCompiledForDraw())
            {
                continue;
            }

            RHIShader* shader = material->GetShader()->GetRHIShader().get();
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
            // Bind IBL after material textures (units 0–3) so cubemap samplers on 4–6 stay active.
            if (bindPBRIBL && sceneBinding.IBLEnvironment != nullptr)
            {
                sceneBinding.IBLEnvironment->BindForPBRDraw(*shader);
            }
            DrawMeshCommand(drawCommand);
        }
    }
}
