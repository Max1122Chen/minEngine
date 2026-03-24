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

        virtual void Execute() override;
        
    public:
        std::vector<MeshDrawCommand> m_DrawCommands;

    private:
        virtual void Render() override;
        void SortDrawCommands();
    };
}