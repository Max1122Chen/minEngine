#pragma once
#include "Core.h"
#include "RenderPassBase.h"

namespace minEngine
{
    class PostProcessPass : public RenderPassBase
    {
    public:
        PostProcessPass() = default;
        virtual ~PostProcessPass() = default;

        virtual void Execute() override;
        virtual void Render() override;


    };
}
