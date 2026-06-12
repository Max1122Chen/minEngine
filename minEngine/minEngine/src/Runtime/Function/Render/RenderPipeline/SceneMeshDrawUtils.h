#pragma once

#include "Core.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"

#include <vector>

namespace minEngine
{
    class RenderPipeline;
    class RHICommandList;

    enum class MeshPassKind : uint8_t
    {
        Opaque,
        Translucent,
    };

    void PrepareSceneMeshDrawPackets(
        RenderPipeline& pipeline,
        RHICommandList& cmdList,
        const std::vector<MeshDrawCommand>& drawCommands,
        MeshPassKind passKind,
        std::vector<MeshDrawPacket>& outPackets);

    void SubmitSceneMeshDrawPackets(
        RenderPipeline& pipeline,
        RHICommandList& cmdList,
        const std::vector<MeshDrawCommand>& drawCommands,
        std::vector<MeshDrawPacket>& drawPackets);
}
