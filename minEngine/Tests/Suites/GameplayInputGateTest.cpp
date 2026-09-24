#include "GameplayInputGateTest.h"
#include "Access/InputSystemTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/SceneViewport.h"

namespace minEngine
{
    class GameplayInputGateTestScope
    {
    public:
        GameplayInputGateTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            Testing::TestAccess<InputSystem>::SetInstance(&m_InputSystem);
        }

        ~GameplayInputGateTestScope()
        {
            Testing::TestAccess<InputSystem>::SetInstance(nullptr);

            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);

            m_ObjectManager.Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

        InputSystem& GetInput() { return m_InputSystem; }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
        InputSystem m_InputSystem;
    };

    namespace
    {
        bool RunStandaloneEnabledWithoutEditorViewport()
        {
            GameplayInputGateTestScope scope;
            if (!scope.GetInput().IsGameplayInputEnabled())
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: standalone without viewport should enable input.");
                return false;
            }
            return true;
        }

        bool RunEditorViewportBlocksUntilBeginPlay()
        {
            GameplayInputGateTestScope scope;

            SceneViewport dummyViewport;
            SceneManager::Get().SetEditorSceneViewport(&dummyViewport);

            if (scope.GetInput().IsGameplayInputEnabled())
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: Edit with viewport should disable gameplay input.");
                return false;
            }

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("input-gate");
            scene->DispatchBeginPlay();

            if (!scope.GetInput().IsGameplayInputEnabled())
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: after DispatchBeginPlay should enable gameplay input.");
                return false;
            }

            scene->DispatchEndPlay(EEndPlayReason::Stopped);
            if (scope.GetInput().IsGameplayInputEnabled())
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: after EndPlay with viewport should disable again.");
                return false;
            }

            SceneManager::Get().SetEditorSceneViewport(nullptr);
            return true;
        }

        bool RunGatedTickStillClearsMouseScroll()
        {
            GameplayInputGateTestScope scope;

            SceneViewport dummyViewport;
            SceneManager::Get().SetEditorSceneViewport(&dummyViewport);

            if (scope.GetInput().IsGameplayInputEnabled())
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: Edit viewport required for scroll-cleanup case.");
                SceneManager::Get().SetEditorSceneViewport(nullptr);
                return false;
            }

            Testing::TestAccess<InputSystem>::SimulateMouseScroll(scope.GetInput(), -1.0f);
            if (!Testing::TestAccess<InputSystem>::IsMouseScrollDown(scope.GetInput()))
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: scroll inject should mark MouseScroll Down.");
                SceneManager::Get().SetEditorSceneViewport(nullptr);
                return false;
            }

            Testing::TestAccess<InputSystem>::Tick(scope.GetInput(), 1.0f / 60.0f);
            if (Testing::TestAccess<InputSystem>::IsMouseScrollDown(scope.GetInput()))
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: gated Tick must clear MouseScroll (BUG-CORE-004).");
                SceneManager::Get().SetEditorSceneViewport(nullptr);
                return false;
            }

            SceneManager::Get().SetEditorSceneViewport(nullptr);
            return true;
        }

        bool RunCaptureFlagGatesWhilePlaying()
        {
            GameplayInputGateTestScope scope;

            SceneViewport dummyViewport;
            SceneManager::Get().SetEditorSceneViewport(&dummyViewport);

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("input-gate-capture");
            scene->DispatchBeginPlay();

            if (!scope.GetInput().IsGameplayInputEnabled())
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: after BeginPlay with default capture should enable.");
                SceneManager::Get().SetEditorSceneViewport(nullptr);
                return false;
            }

            scope.GetInput().SetEditorGameplayCaptureActive(false);
            if (scope.GetInput().IsGameplayInputEnabled())
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: capture false while Playing must disable IMC (ED-F23).");
                SceneManager::Get().SetEditorSceneViewport(nullptr);
                return false;
            }

            scope.GetInput().SetEditorGameplayCaptureActive(true);
            if (!scope.GetInput().IsGameplayInputEnabled())
            {
                ME_LOG(LogTest, Error, "GameplayInputGate: capture true while Playing must enable IMC.");
                SceneManager::Get().SetEditorSceneViewport(nullptr);
                return false;
            }

            SceneManager::Get().SetEditorSceneViewport(nullptr);
            return true;
        }
    }

    bool RunGameplayInputGateTests()
    {
        return RunStandaloneEnabledWithoutEditorViewport()
            && RunEditorViewportBlocksUntilBeginPlay()
            && RunGatedTickStillClearsMouseScroll()
            && RunCaptureFlagGatesWhilePlaying();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("gameplay-input-gate: editor viewport and begin-play [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunGameplayInputGateTests());
}
