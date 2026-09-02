#include "Runtime/Core/Command/BuiltinCommands.h"
#include "Runtime/Core/Command/CommandExecutor.h"
#include "Runtime/Core/Command/CommandRegistry.h"
#include "Runtime/Core/Command/CompletionService.h"
#include "Runtime/Core/Command/SetValueValidation.h"
#include "Runtime/Core/PropertyPath/PropertyPath.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/ReflectionSample.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/PointLightComponent.h"
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
    bool ResultContainsText(const minEngine::Command::CommandResult& result, std::string_view needle)
    {
        if (result.Message.find(needle) != std::string::npos)
        {
            return true;
        }

        for (const minEngine::Command::CommandOutputLine& line : result.Lines)
        {
            for (const minEngine::Command::CommandOutputSegment& segment : line.Segments)
            {
                if (segment.Text.find(needle) != std::string::npos)
                {
                    return true;
                }
            }
        }

        return false;
    }

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

    std::shared_ptr<minEngine::Scene> CreateSampleEnumScene()
    {
        const std::shared_ptr<minEngine::Scene> scene =
            minEngine::SceneManager::Get().CreateNewScene("command-system-enum-test");
        const std::shared_ptr<minEngine::GameObject> sampleObject = scene->CreateGameObject();
        sampleObject->Rename("Sample");
        sampleObject->AddComponent<minEngine::ReflectionSampleComponent>();
        return scene;
    }

    bool CompletionContainsInsertText(
        const std::vector<minEngine::Command::CompletionItem>& items,
        std::string_view insertText)
    {
        for (const minEngine::Command::CompletionItem& item : items)
        {
            if (item.InsertText == insertText)
            {
                return true;
            }
        }

        return false;
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
    CHECK(minEngine::Command::CommandRegistry::Get().Find("find") != nullptr);
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

TEST_CASE("command-system: find matches by substring name [full]")
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
    const minEngine::Command::CommandResult result = executor.ExecuteLine("find Sun", context);

    CHECK(result.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(ResultContainsText(result, "Sun"));
    CHECK(result.Message.find("1 match") != std::string::npos);
}

TEST_CASE("command-system: find matches by type query [full]")
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
    const minEngine::Command::CommandResult result =
        executor.ExecuteLine("find type=DirectionalLightComponent", context);

    CHECK(result.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(ResultContainsText(result, "Sun"));
}

TEST_CASE("command-system: find matches by exact name query [full]")
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
    const minEngine::Command::CommandResult exactResult = executor.ExecuteLine("find name=Sun", context);
    CHECK(exactResult.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(ResultContainsText(exactResult, "Sun"));

    const minEngine::Command::CommandResult noMatchResult = executor.ExecuteLine("find name=sun", context);
    CHECK(noMatchResult.Status == minEngine::Command::CommandStatus::Error);
    CHECK(ResultContainsText(noMatchResult, "No matches"));
}

TEST_CASE("command-system: set updates bool property [full]")
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
    const minEngine::Command::CommandResult setTrueResult =
        executor.ExecuteLine("set Sun.m_CastShadow true", context);
    CHECK(setTrueResult.Status == minEngine::Command::CommandStatus::Ok);

    minEngine::GameObject* sunObject = scene->GetAllGameObjects().front().get();
    const std::vector<std::shared_ptr<minEngine::DirectionalLightComponent>> lights =
        sunObject->GetComponentsOfType<minEngine::DirectionalLightComponent>();
    REQUIRE_FALSE(lights.empty());
    CHECK(lights.front()->CastShadow() == true);

    const minEngine::Command::CommandResult setFalseResult =
        executor.ExecuteLine("set Sun.m_CastShadow = false", context);
    CHECK(setFalseResult.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(lights.front()->CastShadow() == false);

    const minEngine::Command::CommandResult invalidBoolResult =
        executor.ExecuteLine("set Sun.m_CastShadow maybe", context);
    CHECK(invalidBoolResult.Status == minEngine::Command::CommandStatus::Error);
    CHECK(invalidBoolResult.Message.find("expected bool") != std::string::npos);
}

TEST_CASE("command-system: set value completion suggests bool literals [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::Command::CompletionItem> allBoolItems =
        minEngine::Command::CompletionService::Complete("set Sun.m_CastShadow ", 0, context);
    CHECK(CompletionContainsInsertText(allBoolItems, "true"));
    CHECK(CompletionContainsInsertText(allBoolItems, "false"));

    const std::vector<minEngine::Command::CompletionItem> filteredBoolItems =
        minEngine::Command::CompletionService::Complete("set Sun.m_CastShadow tr", 0, context);
    CHECK(CompletionContainsInsertText(filteredBoolItems, "true"));
    CHECK_FALSE(CompletionContainsInsertText(filteredBoolItems, "false"));
}

TEST_CASE("command-system: set value completion suggests enum literals [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSampleEnumScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::Command::CompletionItem> enumItems =
        minEngine::Command::CompletionService::Complete("set Sample.SampleData.EnumField ", 0, context);
    CHECK(CompletionContainsInsertText(enumItems, "ValueA"));
    CHECK(CompletionContainsInsertText(enumItems, "ValueB"));
    CHECK(CompletionContainsInsertText(enumItems, "ValueC"));
}

TEST_CASE("command-system: set value validation colors bool and numeric input [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    const minEngine::Command::PropertyValueValidation partialBool =
        minEngine::Command::SetValueValidation::ValidateInputLine(context, "set Sun.m_CastShadow tr");
    CHECK(partialBool.State == minEngine::Command::PropertyValueValidationState::Partial);

    const minEngine::Command::PropertyValueValidation validBool =
        minEngine::Command::SetValueValidation::ValidateInputLine(context, "set Sun.m_CastShadow true");
    CHECK(validBool.State == minEngine::Command::PropertyValueValidationState::Valid);

    const minEngine::Command::PropertyValueValidation invalidFloat =
        minEngine::Command::SetValueValidation::ValidateInputLine(context, "set Sun.m_Intensity foo");
    CHECK(invalidFloat.State == minEngine::Command::PropertyValueValidationState::Invalid);

    const minEngine::Command::PropertyValueValidation partialFloat =
        minEngine::Command::SetValueValidation::ValidateInputLine(context, "set Sun.m_Intensity 3.");
    CHECK(partialFloat.State == minEngine::Command::PropertyValueValidationState::Partial);
}

TEST_CASE("command-system: set updates enum property [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSampleEnumScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult setResult =
        executor.ExecuteLine("set Sample.SampleData.EnumField ValueC", context);
    CHECK(setResult.Status == minEngine::Command::CommandStatus::Ok);

    const minEngine::Command::CommandResult getResult =
        executor.ExecuteLine("get Sample.SampleData.EnumField", context);
    CHECK(getResult.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(getResult.Message == setResult.Message);
}

TEST_CASE("command-system: set delegates to EditorSetValue hook when provided [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    bool hookInvoked = false;
    context.EditorSetValue = [&](std::string_view propertyPathText, std::string_view valueLiteral) {
        hookInvoked = true;
        CHECK(propertyPathText == "Sun.m_Intensity");
        CHECK(valueLiteral == "3.0");
        return minEngine::Command::CommandResult::MakeOk("editor hook");
    };

    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult result = executor.ExecuteLine("set Sun.m_Intensity 3.0", context);

    CHECK(hookInvoked);
    CHECK(result.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(result.Message == "editor hook");

    minEngine::GameObject* sunObject = scene->GetAllGameObjects().front().get();
    const std::vector<std::shared_ptr<minEngine::DirectionalLightComponent>> lights =
        sunObject->GetComponentsOfType<minEngine::DirectionalLightComponent>();
    REQUIRE_FALSE(lights.empty());
    CHECK(lights.front()->GetIntensity() == doctest::Approx(2.5f));
}

TEST_CASE("command-system: TryBuildSetTransaction captures before and after values [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    const std::optional<minEngine::Command::PropertyPath> propertyPath =
        minEngine::Command::PropertyPath::Parse("Sun.m_Intensity");
    REQUIRE(propertyPath.has_value());

    minEngine::Command::PropertySetTransaction transaction;
    minEngine::Command::CommandResult buildError;
    CHECK(propertyPath->TryBuildSetTransaction(context, "3.0", transaction, buildError));
    CHECK(buildError.Status == minEngine::Command::CommandStatus::Ok);
    CHECK_FALSE(transaction.BeforeValue.empty());
    CHECK_FALSE(transaction.AfterValue.empty());
    CHECK(transaction.BeforeValue != transaction.AfterValue);
    CHECK_FALSE(transaction.OwnerGuid.IsZero());
    CHECK_FALSE(transaction.OwnerClassName.empty());
    CHECK(transaction.PropertySubPath == "m_Intensity");
}

TEST_CASE("command-system: explicit @ property path get and set [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    const std::optional<minEngine::Command::PropertyPath> propertyPath =
        minEngine::Command::PropertyPath::Parse("Sun@DirectionalLightComponent.m_Intensity");
    REQUIRE(propertyPath.has_value());
    CHECK(propertyPath->GetGameObjectName() == "Sun");
    CHECK(propertyPath->GetExplicitComponentName() == "DirectionalLightComponent");
    CHECK(propertyPath->GetPropertySubPath() == "m_Intensity");

    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult setResult =
        executor.ExecuteLine("set Sun@DirectionalLightComponent.m_Intensity 4.0", context);
    CHECK(setResult.Status == minEngine::Command::CommandStatus::Ok);

    const minEngine::Command::CommandResult getResult =
        executor.ExecuteLine("get Sun@DirectionalLightComponent.m_Intensity", context);
    CHECK(getResult.Status == minEngine::Command::CommandStatus::Ok);
    CHECK(getResult.Message.find("4") != std::string::npos);
}

TEST_CASE("command-system: property completion shows reflection type [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::Command::CompletionItem> items =
        minEngine::Command::CompletionService::Complete("set Sun.m_Intensity", 0, context);
    REQUIRE_FALSE(items.empty());

    bool foundIntensity = false;
    for (const minEngine::Command::CompletionItem& item : items)
    {
        if (item.InsertText.find("m_Intensity") != std::string::npos)
        {
            foundIntensity = true;
            CHECK(item.Description == "float");
        }
    }

    CHECK(foundIntensity);
}

TEST_CASE("command-system: @ component completion lists attached types [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::Command::CompletionItem> items =
        minEngine::Command::CompletionService::Complete("get Sun@", 0, context);
    CHECK(CompletionContainsInsertText(items, "Sun@DirectionalLightComponent"));
}

TEST_CASE("command-system: short path ambiguity lists @ candidates [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::Command::CommandRegistry::Get().Clear();
    minEngine::Command::RegisterBuiltinCommands();

    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("command-system-ambiguity-test");
    const std::shared_ptr<minEngine::GameObject> sunObject = scene->CreateGameObject();
    sunObject->Rename("Sun");
    sunObject->AddComponent<minEngine::DirectionalLightComponent>();
    sunObject->AddComponent<minEngine::PointLightComponent>();

    minEngine::Command::CommandContext context;
    context.ActiveScene = scene.get();

    minEngine::Command::CommandExecutor executor;
    const minEngine::Command::CommandResult getResult =
        executor.ExecuteLine("get Sun.m_Intensity", context);
    CHECK(getResult.Status == minEngine::Command::CommandStatus::Error);
    CHECK(ResultContainsText(getResult, "ambiguous"));
    CHECK(ResultContainsText(getResult, "Sun@DirectionalLightComponent.m_Intensity"));
    CHECK(ResultContainsText(getResult, "Sun@PointLightComponent.m_Intensity"));

    const std::vector<minEngine::Command::CompletionItem> items =
        minEngine::Command::CompletionService::Complete("set Sun.m_Intensity", 0, context);
    CHECK(CompletionContainsInsertText(items, "Sun@DirectionalLightComponent.m_Intensity"));
    CHECK(CompletionContainsInsertText(items, "Sun@PointLightComponent.m_Intensity"));
}
