#pragma once
#include "Core.h"
#include "RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

namespace minEngine
{
    class RenderCamera;
    class RHICommandList;

    class TranslucencyPass : public RenderPassBase
    {
    public:
        TranslucencyPass() = default;
        virtual ~TranslucencyPass() = default;

        virtual void Execute() override;
        void Execute(RHICommandList& cmdList);

    public:
        std::vector<MeshDrawCommand> m_DrawCommands;
        RenderCamera* m_SortCamera = nullptr;
        ShadowResourceHandle m_DirectionalShadowHandle;
        std::vector<ShadowResourceHandle> m_SpotShadowHandles;
        std::vector<ShadowResourceHandle> m_PointShadowHandles;

    private:
        void Render(RHICommandList& cmdList);
        void SortDrawCommands();
    };
}