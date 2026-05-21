#pragma once
#include "Core.h"
#include "RenderPassBase.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

namespace minEngine
{
    class RenderCamera;

    class TranslucencyPass : public RenderPassBase
    {
    public:
        TranslucencyPass() = default;
        virtual ~TranslucencyPass() = default;

        virtual void Execute() override;
        
    public:
        std::vector<MeshDrawCommand> m_DrawCommands;
        RenderCamera* m_SortCamera = nullptr;

    private:
        virtual void Render() override;
        void SortDrawCommands();
    };
}