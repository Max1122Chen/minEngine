#pragma once
#include "Core.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <vector>

namespace minEngine
{
    class RenderPipeline;
    class EngineSceneBindingSets;
    class RHIGraphicsPipelineState;
    class RHICommandList;

    enum class MeshPassKind : uint8_t
    {
        Opaque,
        Translucent,
    };

    struct MeshPassSceneBinding
    {
        const MeshDrawCommand& DrawCommand;
        bool bBindLighting = false;
        bool bBindPBRIBL = false;
        const ShadowResourceHandle* DirectionalShadowHandle = nullptr;
        const std::vector<ShadowResourceHandle>* SpotShadowHandles = nullptr;
        const std::vector<ShadowResourceHandle>* PointShadowHandles = nullptr;
    };

    class RenderPassBase
    {
    public:
        RenderPassBase() = default;
        virtual ~RenderPassBase() = default;

        virtual void Execute() = 0;

    protected:
        void PrepareMeshDrawCommands(
            RHICommandList& cmdList,
            std::vector<MeshDrawCommand>& drawCommands,
            MeshPassKind passKind);
        void DrawMeshCommand(RHICommandList& cmdList, const MeshDrawCommand& drawCommand);
        void BindSceneDrawResources(
            RHICommandList& cmdList,
            const RenderPipeline& pipeline,
            const MeshPassSceneBinding& binding);

    public:
        RenderPipeline* pipeline = nullptr;
    };
}
