#include "Core.h"
#include "Application.h"
#include "Runtime/Engine.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManagerTest.h"
#include "Runtime/Function/Render/Material/MaterialIR/MaterialIRTest.h"
#include "Runtime/Core/Serialization/SerializationArchiveTest.h"

extern minEngine::Application* minEngine::CreateApplication();

int main(int argc, char** argv)
{
    if (minEngine::ShouldRunObjectManagerTestsOnly(argc, argv))
    {
        minEngine::LogSystem::Initialize();
        const bool passed = minEngine::RunObjectManagerTests(argc, argv);
        minEngine::LogSystem::Shutdown();
        return passed ? 0 : 1;
    }

    if (minEngine::ShouldRunSerializationArchiveTestsOnly(argc, argv))
    {
        minEngine::LogSystem::Initialize();
        const bool passed = minEngine::RunSerializationArchiveTests(argc, argv);
        minEngine::LogSystem::Shutdown();
        return passed ? 0 : 1;
    }

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