#pragma once
#include "Core.h"

namespace minEngine
{
    class FrameBuffer;

    class RenderPassBase
    {
    public:
        RenderPassBase() = default;
        virtual ~RenderPassBase() = default;

        virtual void Execute() = 0;

    protected:
        virtual void Render() = 0;

    public:
        FrameBuffer* m_FrameBuffer = nullptr;
    };
}