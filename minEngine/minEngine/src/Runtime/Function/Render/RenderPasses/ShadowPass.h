#pragma once
#include "Core.h"
#include "RenderPassBase.h"

namespace minEngine
{
    class UniformBuffer;
    class FrameBuffer;
    class RHITexture2D;

    class ShadowPass : public RenderPassBase
    {
    public:
        ShadowPass() = default;
        virtual ~ShadowPass() = default;    

        virtual void Execute() override;
    
    private:
        virtual void Render() override;

    private:
        
    };
}