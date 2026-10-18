#include "InputKeyCatalogTest.h"
#include "Access/InputSystemTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Input/InputKeys.h"
#include "Runtime/Function/Input/InputKeyTypes.h"

namespace minEngine
{
    class InputKeyCatalogTestScope
    {
    public:
        InputKeyCatalogTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            Testing::TestAccess<InputSystem>::SetInstance(&m_InputSystem);
            m_InputSystem.Initialize();
        }

        ~InputKeyCatalogTestScope()
        {
            m_InputSystem.Shutdown();
            Testing::TestAccess<InputSystem>::SetInstance(nullptr);

            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);

            ObjectManager::Shutdown();
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
        bool RunCatalogContainsExtendedKeys()
        {
            InputKeyCatalogTestScope scope;
            InputSystem& input = scope.GetInput();

            const InputKey samples[] = {
                InputKeys::Mouse_4,
                InputKeys::Mouse_8,
                InputKeys::Key_F24,
                InputKeys::Key_Insert,
                InputKeys::Key_KP_Enter,
                InputKeys::Key_LeftSuper,
                InputKeys::Key_Super,
                InputKeys::Gamepad_A,
                InputKeys::Gamepad_LeftTrigger,
            };

            for (const InputKey& key : samples)
            {
                // Touching GetKeyState via TestAccess proves the slot exists after Initialize.
                (void)Testing::TestAccess<InputSystem>::GetKeyAction(input, key);
            }

            if (!(InputKeys::Gamepad_Cross == InputKeys::Gamepad_A)
                || !(InputKeys::Gamepad_Circle == InputKeys::Gamepad_B)
                || !(InputKeys::Gamepad_Square == InputKeys::Gamepad_X)
                || !(InputKeys::Gamepad_Triangle == InputKeys::Gamepad_Y))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: PS face aliases must share canonical Names.");
                return false;
            }

            return true;
        }

        bool RunSyntheticShiftAggregation()
        {
            InputKeyCatalogTestScope scope;
            InputSystem& input = scope.GetInput();

            if (Testing::TestAccess<InputSystem>::KeyDown(input, InputKeys::Key_Shift))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: Key_Shift should start idle.");
                return false;
            }

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_LeftShift, InputKeyAction::Press);
            if (!Testing::TestAccess<InputSystem>::KeyDown(input, InputKeys::Key_Shift))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: LeftShift press should make Key_Shift held.");
                return false;
            }
            if (Testing::TestAccess<InputSystem>::GetKeyAction(input, InputKeys::Key_Shift) != InputKeyAction::Press)
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: Key_Shift rising edge should be Press.");
                return false;
            }

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_RightShift, InputKeyAction::Press);
            if (!Testing::TestAccess<InputSystem>::KeyDown(input, InputKeys::Key_Shift))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: Key_Shift must stay held with both sides down.");
                return false;
            }

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_LeftShift, InputKeyAction::Release);
            if (!Testing::TestAccess<InputSystem>::KeyDown(input, InputKeys::Key_Shift))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: Key_Shift must stay held while RightShift remains.");
                return false;
            }

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_RightShift, InputKeyAction::Release);
            if (Testing::TestAccess<InputSystem>::KeyDown(input, InputKeys::Key_Shift))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: Key_Shift should release when both sides up.");
                return false;
            }

            return true;
        }

        bool RunSyntheticControlAndSuper()
        {
            InputKeyCatalogTestScope scope;
            InputSystem& input = scope.GetInput();

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_LeftControl, InputKeyAction::Press);
            if (!Testing::TestAccess<InputSystem>::KeyDown(input, InputKeys::Key_Control))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: Key_Control synthetic failed.");
                return false;
            }
            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_LeftControl, InputKeyAction::Release);

            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_RightSuper, InputKeyAction::Press);
            if (!Testing::TestAccess<InputSystem>::KeyDown(input, InputKeys::Key_Super))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: Key_Super synthetic failed.");
                return false;
            }
            Testing::TestAccess<InputSystem>::SimulateKey(input, InputKeys::Key_RightSuper, InputKeyAction::Release);

            return true;
        }

        bool RunGamepadIdleWithoutDevice()
        {
            InputKeyCatalogTestScope scope;
            InputSystem& input = scope.GetInput();

            Testing::TestAccess<InputSystem>::Tick(input, 1.0f / 60.0f);
            if (Testing::TestAccess<InputSystem>::KeyDown(input, InputKeys::Gamepad_A))
            {
                ME_LOG(LogTest, Error, "InputKeyCatalog: Gamepad_A should stay idle without a pad.");
                return false;
            }

            return true;
        }
    }

    bool RunInputKeyCatalogTests()
    {
        return RunCatalogContainsExtendedKeys()
            && RunSyntheticShiftAggregation()
            && RunSyntheticControlAndSuper()
            && RunGamepadIdleWithoutDevice();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("input-key-catalog: synthetic modifiers and catalog [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunInputKeyCatalogTests());
}
