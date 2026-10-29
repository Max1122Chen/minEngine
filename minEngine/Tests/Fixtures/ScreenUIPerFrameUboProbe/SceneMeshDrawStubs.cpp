// The probe drives the screen-space pass's uniform publication, not its draw submission, so the
// mesh-draw helpers it calls are stubbed. Nothing else in ScreenUIPass.cpp reaches them.

#include "Runtime/Function/Render/RenderPipeline/SceneMeshDrawUtils.h"

#include <cstdio>

namespace minEngine
{
    void PrepareSceneMeshDrawPackets(
        ForwardRenderer& pipeline,
        RHICommandList& cmdList,
        const std::vector<MeshDrawCommand>& drawCommands,
        MeshPassKind passKind,
        std::vector<MeshDrawPacket>& outPackets)
    {
        (void)pipeline;
        (void)cmdList;
        (void)drawCommands;
        (void)passKind;
        outPackets.clear();
    }

    void SubmitSceneMeshDrawPackets(
        ForwardRenderer& pipeline,
        RHICommandList& cmdList,
        const std::vector<MeshDrawCommand>& drawCommands,
        std::vector<MeshDrawPacket>& drawPackets)
    {
        (void)pipeline;
        (void)cmdList;
        (void)drawCommands;
        (void)drawPackets;
    }
}
