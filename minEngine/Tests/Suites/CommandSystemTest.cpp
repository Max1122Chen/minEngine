#include "DebugCommand/DebugBuiltinCommands.h"
#include "DebugCommand/DebugCommandExecutor.h"
#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandCompletionService.h"
#include "DebugCommand/DebugCommandSetValueValidation.h"
#include "PropertyPath/PropertyPath.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/ReflectionSample.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/PointLightComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "EngineTestFixture.h"

#include "doctest.h"

namespace minEngine
{
    class CommandSystemTestScope
    {
    public:
        CommandSystemTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~CommandSystemTestScope()
        {
            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);

            m_ObjectManager.Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };
}

namespace
{
    bool ResultContainsText(const minEngine::DebugCommand::DebugCommandResult& result, std::string_view needle)
    {
        if (result.Message.find(needle) != std::string::npos)
        {
            return true;
        }

        for (const minEngine::DebugCommand::DebugCommandOutputLine& line : result.Lines)
        {
            for (const minEngine::DebugCommand::DebugCommandOutputSegment& segment : line.Segments)
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
        const std::vector<minEngine::DebugCommand::CompletionItem>& items,
        std::string_view insertText)
    {
        for (const minEngine::DebugCommand::CompletionItem& item : items)
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
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    minEngine::DebugCommand::DebugCommandContext context;
    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("help", context);

    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK_FALSE(result.Lines.empty());
    CHECK(minEngine::DebugCommand::DebugCommandRegistry::Get().Find("get") != nullptr);
    CHECK(minEngine::DebugCommand::DebugCommandRegistry::Get().Find("set") != nullptr);
    CHECK(minEngine::DebugCommand::DebugCommandRegistry::Get().Find("inspect") != nullptr);
    CHECK(minEngine::DebugCommand::DebugCommandRegistry::Get().Find("find") != nullptr);
}

TEST_CASE("command-system: unknown command returns error [full]")
{
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    minEngine::DebugCommand::DebugCommandContext context;
    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("not_a_command", context);

    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
}

TEST_CASE("command-system: get reads component primitive property [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("get Sun.m_Intensity", context);

    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(result.Message.find("2.5") != std::string::npos);
    CHECK(result.PayloadJson.find("\"op\":\"get\"") != std::string::npos);
    CHECK(result.PayloadJson.find("2.5") != std::string::npos);
}

TEST_CASE("command-system: set updates component primitive property [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult setResult = executor.ExecuteLine("set Sun.m_Intensity 3.0", context);
    CHECK(setResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);

    minEngine::GameObject* sunObject = scene->GetAllGameObjects().front().get();
    const std::vector<std::shared_ptr<minEngine::DirectionalLightComponent>> lights =
        sunObject->GetComponentsOfType<minEngine::DirectionalLightComponent>();
    REQUIRE_FALSE(lights.empty());
    CHECK(lights.front()->GetIntensity() == doctest::Approx(3.0f));

    const minEngine::DebugCommand::DebugCommandResult getResult = executor.ExecuteLine("get Sun.m_Intensity", context);
    CHECK(getResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(getResult.Message.find("3") != std::string::npos);
}

TEST_CASE("command-system: set rejects invalid float literal [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("set Sun.m_Intensity foo", context);

    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(result.Message.find("expected float") != std::string::npos);
}

TEST_CASE("command-system: inspect lists object and component fields [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("inspect Sun", context);

    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(result.Message.find("Sun") != std::string::npos);
    CHECK(result.Message.find("Intensity") != std::string::npos);
    CHECK(result.Message.find("DirectionalLightComponent") != std::string::npos);
}

TEST_CASE("command-system: find matches by substring name [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("find Sun", context);

    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(ResultContainsText(result, "Sun"));
    CHECK(result.Message.find("1 match") != std::string::npos);
    CHECK(result.PayloadJson.find("\"op\":\"find\"") != std::string::npos);
    CHECK(result.PayloadJson.find("\"count\":1") != std::string::npos);
    CHECK(result.PayloadJson.find("Sun") != std::string::npos);
}

TEST_CASE("command-system: find matches by type query [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result =
        executor.ExecuteLine("find type=DirectionalLightComponent", context);

    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(ResultContainsText(result, "Sun"));
}

TEST_CASE("command-system: find matches by exact name query [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult exactResult = executor.ExecuteLine("find name=Sun", context);
    CHECK(exactResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(ResultContainsText(exactResult, "Sun"));

    const minEngine::DebugCommand::DebugCommandResult noMatchResult = executor.ExecuteLine("find name=sun", context);
    CHECK(noMatchResult.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(ResultContainsText(noMatchResult, "No matches"));
}

TEST_CASE("command-system: set updates bool property [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult setTrueResult =
        executor.ExecuteLine("set Sun.m_CastShadow true", context);
    CHECK(setTrueResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);

    minEngine::GameObject* sunObject = scene->GetAllGameObjects().front().get();
    const std::vector<std::shared_ptr<minEngine::DirectionalLightComponent>> lights =
        sunObject->GetComponentsOfType<minEngine::DirectionalLightComponent>();
    REQUIRE_FALSE(lights.empty());
    CHECK(lights.front()->CastShadow() == true);

    const minEngine::DebugCommand::DebugCommandResult setFalseResult =
        executor.ExecuteLine("set Sun.m_CastShadow = false", context);
    CHECK(setFalseResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(lights.front()->CastShadow() == false);

    const minEngine::DebugCommand::DebugCommandResult invalidBoolResult =
        executor.ExecuteLine("set Sun.m_CastShadow maybe", context);
    CHECK(invalidBoolResult.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(invalidBoolResult.Message.find("expected bool") != std::string::npos);
}

TEST_CASE("command-system: set value completion suggests bool literals [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::DebugCommand::CompletionItem> allBoolItems =
        minEngine::DebugCommand::DebugCommandCompletionService::Complete("set Sun.m_CastShadow ", 0, context);
    CHECK(CompletionContainsInsertText(allBoolItems, "true"));
    CHECK(CompletionContainsInsertText(allBoolItems, "false"));

    const std::vector<minEngine::DebugCommand::CompletionItem> filteredBoolItems =
        minEngine::DebugCommand::DebugCommandCompletionService::Complete("set Sun.m_CastShadow tr", 0, context);
    CHECK(CompletionContainsInsertText(filteredBoolItems, "true"));
    CHECK_FALSE(CompletionContainsInsertText(filteredBoolItems, "false"));
}

TEST_CASE("command-system: set value completion suggests enum literals [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSampleEnumScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::DebugCommand::CompletionItem> enumItems =
        minEngine::DebugCommand::DebugCommandCompletionService::Complete("set Sample.SampleData.EnumField ", 0, context);
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
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    const minEngine::DebugCommand::PropertyValueValidation partialBool =
        minEngine::DebugCommand::DebugCommandSetValueValidation::ValidateInputLine(context, "set Sun.m_CastShadow tr");
    CHECK(partialBool.State == minEngine::DebugCommand::PropertyValueValidationState::Partial);

    const minEngine::DebugCommand::PropertyValueValidation validBool =
        minEngine::DebugCommand::DebugCommandSetValueValidation::ValidateInputLine(context, "set Sun.m_CastShadow true");
    CHECK(validBool.State == minEngine::DebugCommand::PropertyValueValidationState::Valid);

    const minEngine::DebugCommand::PropertyValueValidation invalidFloat =
        minEngine::DebugCommand::DebugCommandSetValueValidation::ValidateInputLine(context, "set Sun.m_Intensity foo");
    CHECK(invalidFloat.State == minEngine::DebugCommand::PropertyValueValidationState::Invalid);
    CHECK(invalidFloat.Message.find("expected") != std::string::npos);
    CHECK(invalidFloat.Message.find("foo") != std::string::npos);
    CHECK(invalidFloat.Suggestions.empty());

    const minEngine::DebugCommand::PropertyValueValidation partialFloat =
        minEngine::DebugCommand::DebugCommandSetValueValidation::ValidateInputLine(context, "set Sun.m_Intensity 3.");
    CHECK(partialFloat.State == minEngine::DebugCommand::PropertyValueValidationState::Partial);
}

TEST_CASE("command-system: set updates enum property [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSampleEnumScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult setResult =
        executor.ExecuteLine("set Sample.SampleData.EnumField ValueC", context);
    CHECK(setResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);

    const minEngine::DebugCommand::DebugCommandResult getResult =
        executor.ExecuteLine("get Sample.SampleData.EnumField", context);
    CHECK(getResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(getResult.Message == setResult.Message);
}

TEST_CASE("command-system: set delegates to EditorSetValue hook when provided [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    bool hookInvoked = false;
    context.EditorSetValue = [&](std::string_view propertyPathText, std::string_view valueLiteral) {
        hookInvoked = true;
        CHECK(propertyPathText == "Sun.m_Intensity");
        CHECK(valueLiteral == "3.0");
        return minEngine::DebugCommand::DebugCommandResult::MakeOk("editor hook");
    };

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("set Sun.m_Intensity 3.0", context);

    CHECK(hookInvoked);
    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
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
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    const std::optional<minEngine::DebugCommand::PropertyPath> propertyPath =
        minEngine::DebugCommand::PropertyPath::Parse("Sun.m_Intensity");
    REQUIRE(propertyPath.has_value());

    minEngine::DebugCommand::PropertySetTransaction transaction;
    minEngine::DebugCommand::DebugCommandResult buildError;
    CHECK(propertyPath->TryBuildSetTransaction(context, "3.0", transaction, buildError));
    CHECK(buildError.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
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
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    const std::optional<minEngine::DebugCommand::PropertyPath> propertyPath =
        minEngine::DebugCommand::PropertyPath::Parse("Sun@DirectionalLightComponent.m_Intensity");
    REQUIRE(propertyPath.has_value());
    CHECK(propertyPath->GetGameObjectName() == "Sun");
    CHECK(propertyPath->GetExplicitComponentName() == "DirectionalLightComponent");
    CHECK(propertyPath->GetPropertySubPath() == "m_Intensity");

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult setResult =
        executor.ExecuteLine("set Sun@DirectionalLightComponent.m_Intensity 4.0", context);
    CHECK(setResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);

    const minEngine::DebugCommand::DebugCommandResult getResult =
        executor.ExecuteLine("get Sun@DirectionalLightComponent.m_Intensity", context);
    CHECK(getResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(getResult.Message.find("4") != std::string::npos);
}

TEST_CASE("command-system: property completion shows reflection type [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::DebugCommand::CompletionItem> items =
        minEngine::DebugCommand::DebugCommandCompletionService::Complete("set Sun.m_Intensity", 0, context);
    REQUIRE_FALSE(items.empty());

    bool foundIntensity = false;
    for (const minEngine::DebugCommand::CompletionItem& item : items)
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
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::DebugCommand::CompletionItem> items =
        minEngine::DebugCommand::DebugCommandCompletionService::Complete("get Sun@", 0, context);
    CHECK(CompletionContainsInsertText(items, "Sun@DirectionalLightComponent"));
}

TEST_CASE("command-system: short path ambiguity lists @ candidates [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("command-system-ambiguity-test");
    const std::shared_ptr<minEngine::GameObject> sunObject = scene->CreateGameObject();
    sunObject->Rename("Sun");
    sunObject->AddComponent<minEngine::DirectionalLightComponent>();
    sunObject->AddComponent<minEngine::PointLightComponent>();

    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult getResult =
        executor.ExecuteLine("get Sun.m_Intensity", context);
    CHECK(getResult.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(ResultContainsText(getResult, "ambiguous"));
    CHECK(ResultContainsText(getResult, "Sun@DirectionalLightComponent.m_Intensity"));
    CHECK(ResultContainsText(getResult, "Sun@PointLightComponent.m_Intensity"));

    const std::vector<minEngine::DebugCommand::CompletionItem> items =
        minEngine::DebugCommand::DebugCommandCompletionService::Complete("set Sun.m_Intensity", 0, context);
    CHECK(CompletionContainsInsertText(items, "Sun@DirectionalLightComponent.m_Intensity"));
    CHECK(CompletionContainsInsertText(items, "Sun@PointLightComponent.m_Intensity"));
}

TEST_CASE("command-system: set rejects clamped value above ClampMax [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSampleEnumScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result =
        executor.ExecuteLine("set Sample.SampleData.FloatField 11.0", context);
    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(ResultContainsText(result, "ClampMax"));
}

TEST_CASE("command-system: set rejects read-only property [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSampleEnumScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result =
        executor.ExecuteLine("set Sample.SampleData.ReadOnlyIntField 5", context);
    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(ResultContainsText(result, "read-only"));
}

TEST_CASE("command-system: set invalid enum includes suggestions [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSampleEnumScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result =
        executor.ExecuteLine("set Sample.SampleData.EnumField NotAValue", context);
    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(ResultContainsText(result, "suggestions:"));
    CHECK(ResultContainsText(result, "ValueA"));
    CHECK(ResultContainsText(result, "ValueB"));
}

TEST_CASE("command-system: verify asserts property equality [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult okResult =
        executor.ExecuteLine("verify Sun.m_Intensity == 2.5", context);
    CHECK(okResult.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(okResult.PayloadJson.find("\"ok\":true") != std::string::npos);

    const minEngine::DebugCommand::DebugCommandResult okWithoutOperator =
        executor.ExecuteLine("verify Sun.m_Intensity 2.5", context);
    CHECK(okWithoutOperator.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);

    const minEngine::DebugCommand::DebugCommandResult failResult =
        executor.ExecuteLine("verify Sun.m_Intensity == 9.0", context);
    CHECK(failResult.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(failResult.PayloadJson.find("\"ok\":false") != std::string::npos);
}

TEST_CASE("command-system: edit respects EditDefaultsOnly in scene context [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    const std::shared_ptr<minEngine::Scene> scene = CreateSampleEnumScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult editDenied =
        executor.ExecuteLine("edit Sample.SampleData.DefaultsOnlyIntField 8", context);
    CHECK(editDenied.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(ResultContainsText(editDenied, "not editable"));

    const minEngine::DebugCommand::DebugCommandResult setAllowed =
        executor.ExecuteLine("set Sample.SampleData.DefaultsOnlyIntField 8", context);
    CHECK(setAllowed.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);

    const minEngine::DebugCommand::DebugCommandResult editOk =
        executor.ExecuteLine("edit Sample.SampleData.FloatField 4.0", context);
    CHECK(editOk.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
}

TEST_CASE("command-system: help lists edit and verify [full]")
{
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    minEngine::DebugCommand::DebugCommandContext context;
    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("help", context);
    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Ok);
    CHECK(ResultContainsText(result, "edit"));
    CHECK(ResultContainsText(result, "verify"));
}

TEST_CASE("command-system: get requires property path argument [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    minEngine::DebugCommand::DebugCommandRegistry::Get().Clear();
    minEngine::DebugCommand::RegisterBuiltinDebugCommands();

    minEngine::DebugCommand::DebugCommandContext context;
    minEngine::DebugCommand::DebugCommandExecutor executor;
    const minEngine::DebugCommand::DebugCommandResult result = executor.ExecuteLine("get", context);
    CHECK(result.Status == minEngine::DebugCommand::DebugCommandStatus::Error);
    CHECK(ResultContainsText(result, "requires 1 argument"));
}

TEST_CASE("command-system: rename completion lists game objects [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::CommandSystemTestScope scope;
    const std::shared_ptr<minEngine::Scene> scene = CreateSunLightScene();
    minEngine::DebugCommand::DebugCommandContext context;
    context.ActiveScene = scene.get();

    const std::vector<minEngine::DebugCommand::CompletionItem> items =
        minEngine::DebugCommand::DebugCommandCompletionService::Complete("rename Su", 0, context);
    CHECK(CompletionContainsInsertText(items, "Sun"));
}
