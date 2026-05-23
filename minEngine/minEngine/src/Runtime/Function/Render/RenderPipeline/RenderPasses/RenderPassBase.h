#pragma once
#include "Core.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <vector>

namespace minEngine
{
    class RenderPipeline;
    class EngineIBLEnvironment;
    class FrameBuffer;
    class RHIShader;

    struct MeshPassSceneBinding
    {
        const MeshDrawCommand& DrawCommand;
        bool bBindLighting = false;
        bool bBindPBRIBL = false;
        const ShadowResourceHandle* DirectionalShadowHandle = nullptr;
        const std::vector<ShadowResourceHandle>* SpotShadowHandles = nullptr;
        const std::vector<ShadowResourceHandle>* PointShadowHandles = nullptr;
        const EngineIBLEnvironment* IBLEnvironment = nullptr;
    };

    class RenderPassBase
    {
    public:
        RenderPassBase() = default;
        virtual ~RenderPassBase() = default;

        virtual void Execute() = 0;

    protected:
        virtual void Render() = 0;

        void DrawMeshCommand(const MeshDrawCommand& drawCommand);
        void BindSceneDrawResources(RHIShader& shader, const MeshPassSceneBinding& binding);

    public:
        RenderPipeline* pipeline = nullptr;
        FrameBuffer* m_FrameBuffer = nullptr;
    };
}
