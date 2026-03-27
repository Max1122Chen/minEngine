#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

namespace minEngine
{
    struct DirLightShadowEntry;

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

        std::vector<DirLightShadowEntry> m_DirLightShadowEntries;
    };
}