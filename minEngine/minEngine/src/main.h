#include "Core.h"
#include "Application.h"
#include "Runtime/Engine.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/Material/MaterialIR/MaterialIRTest.h"

extern minEngine::Application* minEngine::CreateApplication();

int main(int argc, char** argv)
{
    if (minEngine::ShouldRunMaterialIRTestsOnly(argc, argv))
    {
        minEngine::LogSystem::Initialize();
        const bool passed = minEngine::RunMaterialIRSmokeTests(argc, argv);
        minEngine::LogSystem::Shutdown();
        return passed ? 0 : 1;
    }

    minEngine::Application* app = minEngine::CreateApplication();
    app->Initialize(argc, argv);
    app->Run();
    app->Shutdown();
    delete app;

    return 0;
}