#include "InputTriggerDownTest.h"
#include "Access/InputSystemTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Input/InputAction.h"
#include "Runtime/Function/Input/InputMappingContext.h"
#include "Runtime/Function/Input/InputTriggers.h"
#include "Runtime/Function/Input/InputKeys.h"

#include <memory>

namespace minEngine
{
    class InputTriggerDownTestScope
    {
    public:
        InputTriggerDownTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            Testing::TestAccess<InputSystem>::SetInstance(&m_InputSystem);
            m_InputSystem.Initialize();
        }

        ~InputTriggerDownTestScope()
        {
            m_InputSystem.Shutdown();
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
        bool RunDownStartedAndCanceled()
        {
            InputTriggerDownTestScope scope;
            InputSystem& input = scope.GetInput();

            InputAction sneak("IA_Sneak", InputActionValueType::Boolean);
            sneak.AddTrigger(std::make_shared<InputTriggerDown>());

            InputMappingContext context({
                {&sneak, InputKeys::Key_LeftShift},
            });
            input.AddInputMappingContext(&context, 0);

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_LeftShift, InputKeyAction::Press);
            Testing::TestAccess<InputSystem>::Tick(input, 1.0f / 60.0f);
            if (Testing::TestAccess<InputSystem>::GetTriggerEvent(input, &sneak) != InputTriggerEvent::Started)
            {
                ME_LOG(LogTest, Error, "InputTriggerDown: press should emit Started (CORE-F32 A).");
                return false;
            }

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_LeftShift, InputKeyAction::Down);
            Testing::TestAccess<InputSystem>::Tick(input, 1.0f / 60.0f);
            if (Testing::TestAccess<InputSystem>::GetTriggerEvent(input, &sneak) != InputTriggerEvent::None)
            {
                ME_LOG(LogTest, Error, "InputTriggerDown: hold should not re-emit Started.");
                return false;
            }
            if (Testing::TestAccess<InputSystem>::GetLastTriggerState(input, &sneak) != InputTriggerState::Ongoing)
            {
                ME_LOG(LogTest, Error, "InputTriggerDown: hold LastState should stay Ongoing.");
                return false;
            }

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_LeftShift, InputKeyAction::Release);
            Testing::TestAccess<InputSystem>::Tick(input, 1.0f / 60.0f);
            if (Testing::TestAccess<InputSystem>::GetTriggerEvent(input, &sneak) != InputTriggerEvent::Canceled)
            {
                ME_LOG(LogTest, Error, "InputTriggerDown: release should emit Canceled (CORE-F32 B).");
                return false;
            }

            return true;
        }

        bool RunNoTriggerJumpStillTriggered()
        {
            InputTriggerDownTestScope scope;
            InputSystem& input = scope.GetInput();

            InputAction jump("IA_Jump", InputActionValueType::Boolean);
            InputMappingContext context({
                {&jump, InputKeys::Key_Space},
            });
            input.AddInputMappingContext(&context, 0);

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_Space, InputKeyAction::Press);
            Testing::TestAccess<InputSystem>::Tick(input, 1.0f / 60.0f);
            if (Testing::TestAccess<InputSystem>::GetTriggerEvent(input, &jump) != InputTriggerEvent::Triggered)
            {
                ME_LOG(LogTest, Error, "No-trigger Boolean: press should emit Triggered (Jump regression).");
                return false;
            }

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_Space, InputKeyAction::Down);
            Testing::TestAccess<InputSystem>::Tick(input, 1.0f / 60.0f);
            if (Testing::TestAccess<InputSystem>::GetTriggerEvent(input, &jump) != InputTriggerEvent::Triggered)
            {
                ME_LOG(LogTest, Error, "No-trigger Boolean: hold should keep emitting Triggered.");
                return false;
            }

            return true;
        }
    }

    bool RunInputTriggerDownTests()
    {
        return RunDownStartedAndCanceled() && RunNoTriggerJumpStillTriggered();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("input-trigger-down: Started Canceled and Jump regression [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunInputTriggerDownTests());
}