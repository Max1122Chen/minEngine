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
        
        float CalculateDeltaTime();
    private:
        std::chrono::steady_clock::time_point m_LastTickTimePoint{std::chrono::steady_clock::now()};
    };
}