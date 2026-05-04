#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

namespace minEngine
{
    class RHITexture2DArray;

    class BasePass : public RenderPassBase
    {
    public:
        BasePass() = default;
        virtual ~BasePass() = default;

        virtual void Execute() override;

    private:
        virtual void Render() override;

    public:
        std::vector<MeshDrawCommand> m_DrawCommands;
        ShadowResourceHandle m_DirectionalShadowHandle;
        std::vector<ShadowResourceHandle> m_SpotShadowHandles;
        std::vector<ShadowResourceHandle> m_PointShadowHandles;
    };
}