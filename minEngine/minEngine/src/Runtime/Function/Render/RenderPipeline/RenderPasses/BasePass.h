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
        std::shared_ptr<RHITexture2DArray> m_DirectionalShadowArray;
        ShadowResourceHandle m_DirectionalShadowHandle;
        Matrix4 m_DirectionalLightViewProj = Matrix4(1.0f);
    };
}