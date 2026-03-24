#pragma once
#include "Core.h"
#include "Runtime/Function/Render/RenderPasses/RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

namespace minEngine
{
    class BasePass : public RenderPassBase
    {
    public:
        BasePass() = default;
        virtual ~BasePass() = default;

        std::vector<MeshDrawCommand> m_DrawCommands;

        virtual void Execute() override;
        virtual void Render() override;
    };
}