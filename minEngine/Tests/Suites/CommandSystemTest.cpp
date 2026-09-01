#include "Runtime/Core/Command/BuiltinCommands.h"
#include "Runtime/Core/Command/CommandExecutor.h"
#include "Runtime/Core/Command/CommandRegistry.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include "EngineTestFixture.h"

#include "doctest.h"

namespace minEngine
{
    class CommandSystemTestScope
    {
    public:
        CommandSystemTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~CommandSystemTestScope()
        {
            m_SceneManager.Shutdown();
            SceneManager::SetInstance(nullptr);

            m_ObjectManager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };
}

namespace
{
    std::shared_ptr<minEngine::Scene> CreateSunLightScene()
    {
        const std::shared_ptr<minEngine::Scene> scene = minEngine::SceneManager::Get().CreateNewScene("command-system-test");
        const std::shared_ptr<minEngine::GameObject> sunObject = scene->CreateGameObject();
        sunObject->Rename("Sun");
        const std::shared_ptr<minEngine::DirectionalLightComponent> lightComponent =
            sunObject->AddComponent<minEngine::DirectionalLightComponent>();
        lightComponent->SetIntensity(2.5f);
        return scene;
    }
}

TEST_CASE("command-system: help lists registered commands [full]")
{
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    minEngine::Command::CommandContext context;
    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult result = executor.ExecuteLine("help", context);

    CHECK(result.Status == minEngine::Command::CommandStatus::Ok);
    CHECK_FALSE(result.Lines.empty());
    CHECK(minEngine::Command::CommandRegistry::Get().Find("get") != nullptr);
    CHECK(minEngine::Command::CommandRegistry::Get().Find("set") != nullptr);
    CHECK(minEngine::Command::CommandRegistry::Get().Find("inspect") != nullptr);
}

TEST_CASE("command-system: unknown command returns error [full]")
{
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    minEngine::Command::CommandContext context;
    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult result = executor.ExecuteLine("not_a_command", context);

    CHECK(result.Status == minEngine::Command::CommandStatus::Error);
}

TEST_CASE("command-system: get reads component primitive property [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult result = executor.ExecuteLine("get Sun.m_Intensity", context);

    CHECK(result.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(result.Message.find("2.5") != std::string::npos);
}

TEST_CASE("command-system: set updates component primitive property [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult setResult = executor.ExecuteLine("set Sun.m_Intensity 3.0", context);
    CHECK(setResult.Status == minEngine::Command::CommandStatus::Ok);

    minEngine::GameObject* sunObject = scene->GetAllGameObjects().front().get();
    const std::vector<std::shared_ptr<minEngine::DirectionalLightComponent>> lights =
        sunObject->GetComponentsOfType<minEngine::DirectionalLightComponent>();
    REQUIRE_FALSE(lights.empty());
    CHECK(lights.front()->GetIntensity() == doctest::Approx(3.0f));

    const minEngine::Command::CommandResult getResult = executor.ExecuteLine("get Sun.m_Intensity", context);
    CHECK(getResult.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(getResult.Message.find("3") != std::string::npos);
}

TEST_CASE("command-system: set rejects invalid float literal [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult result = executor.ExecuteLine("set Sun.m_Intensity foo", context);

    CHECK(result.Status == minEngine::Command::CommandStatus::Error);
    CHECK(result.Message.find("expected float") != std::string::npos);
}

TEST_CASE("command-system: inspect lists object and component fields [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult result = executor.ExecuteLine("inspect Sun", context);

    CHECK(result.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(result.Message.find("Sun") != std::string::npos);
    CHECK(result.Message.find("Intensity") != std::string::npos);
    CHECK(result.Message.find("DirectionalLightComponent") != std::string::npos);
}
