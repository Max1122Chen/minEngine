#pragma once
#include "Core.h"
#include "Render/RenderSystem.h"

namespace minEngine
{
    class RenderSystem;

    class Engine
    {
    public:
        Engine() = default;
        ~Engine() = default;

        void Initialize();

        void Shutdown();

        void Run();

    private:
        void TickOneFrame(float deltaTime);
        void LogicalTick(float deltaTime);
        void RendererTick(float deltaTime);

    private:
        std::shared_ptr<RenderSystem> m_RenderSystem;
    };
}