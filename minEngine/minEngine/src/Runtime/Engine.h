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

    public:
        void TickOneFrame(float deltaTime);
        float CalculateDeltaTime();
        float CalculateFPS(float deltaTime);
        
    private:
        void LogicalTick(float deltaTime);
        void RendererTick(float deltaTime);
        
        

    private:
        std::chrono::steady_clock::time_point m_LastTickTimePoint{std::chrono::steady_clock::now()};
    };
}