#pragma once
#include "Core.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

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

    class RenderPassBase
    {
    public:
        RenderPassBase() = default;
        virtual ~RenderPassBase() = default;

        virtual void Execute() = 0;

    protected:
        void PrepareMeshDrawPackets(
            RHICommandList& cmdList,
            const std::vector<MeshDrawCommand>& drawCommands,
            MeshPassKind passKind,
            std::vector<MeshDrawPacket>& outPackets);
        void SubmitSceneMeshDrawPackets(
            RHICommandList& cmdList,
            const std::vector<MeshDrawCommand>& drawCommands,
            std::vector<MeshDrawPacket>& drawPackets);

    public:
        RenderPipeline* pipeline = nullptr;
    };
}

