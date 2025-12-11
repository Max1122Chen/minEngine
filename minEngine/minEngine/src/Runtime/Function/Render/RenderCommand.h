#pragma once
#include "Core.h"

namespace minEngine
{
    class RenderCommand
    {
    public:
        RenderCommand() = default;
        virtual ~RenderCommand() = default;
        
        virtual void Execute() = 0;
    };

    class RenderCommandQueue
    {
    public:
        RenderCommandQueue() = default;
        ~RenderCommandQueue() = default;

        // void EnqueueCommand(const std::shared_ptr<RenderCommand>& command)
        // {
        //     m_Commands.push_back(command);
        // }

    private:
    };
}