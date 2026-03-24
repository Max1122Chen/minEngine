#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RenderPasses/RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

namespace minEngine
{
    class TranslucencyPass : public RenderPassBase
    {
    public:
        TranslucencyPass() = default;
        virtual ~TranslucencyPass() = default;

        std::vector<MeshDrawCommand> m_DrawCommands;

        virtual void Execute() override;
        virtual void Render() override;

    private:
        void SortDrawCommands();
    };
}