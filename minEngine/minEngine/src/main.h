#include "Core.h"
#include "Application.h"
#include "Runtime/Engine.h"
#include "Runtime/Core/CLI/ApplicationCommandLine.h"
#include "Runtime/Core/CLI/CommandLineExitCode.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManagerTest.h"
#include "Runtime/Resource/AssetManagerTest.h"
#include "Runtime/Function/Render/Material/MaterialIR/MaterialIRTest.h"
#include "Runtime/Core/Serialization/SerializationArchiveTest.h"
#include "Runtime/Core/Reflection/ReflectionFunctionTest.h"

extern minEngine::Application* minEngine::CreateApplication();

namespace minEngine
{
    static int ExitCodeFrom(CommandLineExitCode exitCode)
    {
        return static_cast<int>(exitCode);
    }

    static bool RunMaterialIRTestMode(int argc, char** argv)
    {
        LogSystem::Initialize();
        const bool passed = RunMaterialIRSmokeTests(argc, argv);
        LogSystem::Shutdown();
        return passed;
    }
}

int main(int argc, char** argv)
{
    const std::optional<minEngine::CommandLineResult> commandLine =
        minEngine::ApplicationCommandLine::TryParse(argc, argv);
    if (!commandLine.has_value())
    {
        return minEngine::ExitCodeFrom(minEngine::ApplicationCommandLine::GetLastExitCode());
    }

    if (commandLine->RequestedHelp)
    {
        return 0;
    }

    if (commandLine->RequestedVersion)
    {
        return 0;
    }

    if (commandLine->Mode == minEngine::ApplicationMode::Test)
    {
        if (commandLine->TestKind == minEngine::TestRunKind::SingleSuite &&
            commandLine->SuiteId == "material-ir")
        {
            return minEngine::RunMaterialIRTestMode(argc, argv) ? 0 : 1;
        }

        std::fprintf(
            stderr,
            "Test mode '%s' is not available via unified CLI yet (see legacy --*-test flags).\n",
            commandLine->TestKind == minEngine::TestRunKind::SingleSuite ? commandLine->SuiteId.c_str()
                                                                         : (commandLine->TestKind ==
                                                                                    minEngine::TestRunKind::Smoke
                                                                                ? "smoke"
                                                                                : "full"));
        return minEngine::ExitCodeFrom(minEngine::CommandLineExitCode::UsageError);
    }

    if (minEngine::ShouldRunObjectManagerTestsOnly(argc, argv))
    {
        minEngine::LogSystem::Initialize();
        const bool passed = minEngine::RunObjectManagerTests(argc, argv);
        minEngine::LogSystem::Shutdown();
        return passed ? 0 : 1;
    }

    if (minEngine::ShouldRunAssetManagerTestsOnly(argc, argv))
    {
        minEngine::LogSystem::Initialize();
        const bool passed = minEngine::RunAssetManagerTests(argc, argv);
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

    if (minEngine::ShouldRunReflectionFunctionTestsOnly(argc, argv))
    {
        minEngine::LogSystem::Initialize();
        const bool passed = minEngine::RunReflectionFunctionTests(argc, argv);
        minEngine::LogSystem::Shutdown();
        return passed ? 0 : 1;
    }

    minEngine::Application* app = minEngine::CreateApplication();
    app->Initialize(argc, argv, *commandLine);
    app->Run();
    app->Shutdown();
    delete app;

    return 0;
}
