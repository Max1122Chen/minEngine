#include "TranslucencyPass.h"

#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPipeline.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

#include <algorithm>

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
        if (!pipeline)
        {
            return;
        }

        std::vector<MeshDrawPacket> drawPackets;
        PrepareMeshDrawPackets(cmdList, m_DrawCommands, MeshPassKind::Translucent, drawPackets);
        SubmitSceneMeshDrawPackets(cmdList, m_DrawCommands, drawPackets);
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

