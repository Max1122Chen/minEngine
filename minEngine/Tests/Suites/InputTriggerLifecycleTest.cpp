#include "InputTriggerLifecycleTest.h"
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
    class InputTriggerLifecycleTestScope
    {
    public:
        InputTriggerLifecycleTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            Testing::TestAccess<InputSystem>::SetInstance(&m_InputSystem);
            m_InputSystem.Initialize();
        }

        ~InputTriggerLifecycleTestScope()
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
        constexpr float kFrameTime = 1.0f / 60.0f;

        void PressKey(InputSystem& input, const InputKey& key)
        {
            Testing::TestAccess<InputSystem>::SimulateKey(input, key, InputKeyAction::Press);
        }

        void HoldKey(InputSystem& input, const InputKey& key)
        {
            Testing::TestAccess<InputSystem>::SimulateKey(input, key, InputKeyAction::Down);
        }

        void ReleaseKey(InputSystem& input, const InputKey& key)
        {
            Testing::TestAccess<InputSystem>::SimulateKey(input, key, InputKeyAction::Release);
        }

        void TickFrame(InputSystem& input)
        {
            Testing::TestAccess<InputSystem>::Tick(input, kFrameTime);
        }

        InputTriggerEvent EventOf(InputSystem& input, InputAction* action)
        {
            return Testing::TestAccess<InputSystem>::GetTriggerEvent(input, action);
        }

        InputTriggerState StateOf(InputSystem& input, InputAction* action)
        {
            return Testing::TestAccess<InputSystem>::GetLastTriggerState(input, action);
        }

        bool ValueIsZero(InputSystem& input, InputAction* action)
        {
            InputActionValue value = Testing::TestAccess<InputSystem>::GetActionValue(input, action);
            return value.GetMagnitudeSq() <= 0.0f;
        }

        // A pressed action has to be usable again after it was released: three full cycles, each
        // with one Triggered and one Completed, and a zero value in between.
        bool RunPressedCycles()
        {
            InputTriggerLifecycleTestScope scope;
            InputSystem& input = scope.GetInput();

            InputAction fire("IA_FireCycle", InputActionValueType::Boolean);
            fire.AddTrigger(std::make_shared<InputTriggerPressed>());

            InputMappingContext context({{&fire, InputKeys::Key_LeftShift}});
            input.AddInputMappingContext(&context, 0);

            int triggeredCount = 0;
            int completedCount = 0;

            for (int cycle = 0; cycle < 3; ++cycle)
            {
                PressKey(input, InputKeys::Key_LeftShift);
                TickFrame(input);
                if (EventOf(input, &fire) != InputTriggerEvent::Triggered)
                {
                    ME_LOG(LogTest, Error, "Pressed cycle {0}: press should emit Triggered.", cycle);
                    return false;
                }
                if (StateOf(input, &fire) != InputTriggerState::Triggered)
                {
                    ME_LOG(LogTest, Error, "Pressed cycle {0}: press should leave the action Triggered.", cycle);
                    return false;
                }
                ++triggeredCount;

                ReleaseKey(input, InputKeys::Key_LeftShift);
                TickFrame(input);
                if (!ValueIsZero(input, &fire))
                {
                    ME_LOG(LogTest, Error, "Pressed cycle {0}: value must fall to zero on release.", cycle);
                    return false;
                }
                if (EventOf(input, &fire) != InputTriggerEvent::Completed)
                {
                    ME_LOG(LogTest, Error, "Pressed cycle {0}: release should emit Completed.", cycle);
                    return false;
                }
                ++completedCount;
            }

            if (triggeredCount != 3 || completedCount != 3)
            {
                ME_LOG(LogTest, Error, "Pressed cycles: expected 3 Triggered and 3 Completed, saw {0}/{1}.",
                    triggeredCount, completedCount);
                return false;
            }

            return true;
        }

        // Holding the key is one press, not one per frame.
        bool RunPressedHoldFiresOnce()
        {
            InputTriggerLifecycleTestScope scope;
            InputSystem& input = scope.GetInput();

            InputAction fire("IA_FireHold", InputActionValueType::Boolean);
            fire.AddTrigger(std::make_shared<InputTriggerPressed>());

            InputMappingContext context({{&fire, InputKeys::Key_LeftShift}});
            input.AddInputMappingContext(&context, 0);

            PressKey(input, InputKeys::Key_LeftShift);
            TickFrame(input);
            if (EventOf(input, &fire) != InputTriggerEvent::Triggered)
            {
                ME_LOG(LogTest, Error, "Pressed hold: press should emit Triggered.");
                return false;
            }

            int triggeredCount = 1;
            for (int frame = 0; frame < 4; ++frame)
            {
                HoldKey(input, InputKeys::Key_LeftShift);
                TickFrame(input);
                if (EventOf(input, &fire) == InputTriggerEvent::Triggered)
                {
                    ++triggeredCount;
                }
                if (StateOf(input, &fire) == InputTriggerState::None)
                {
                    ME_LOG(LogTest, Error, "Pressed hold: frame {0} must not end the press.", frame);
                    return false;
                }
            }

            if (triggeredCount != 1)
            {
                ME_LOG(LogTest, Error, "Pressed hold: expected a single Triggered, saw {0}.", triggeredCount);
                return false;
            }

            return true;
        }

        // Dedicated check for the stale value: an action that only owns a Pressed trigger must not
        // keep reporting the last press value after the key went up.
        bool RunValueFallsToZeroOnRelease()
        {
            InputTriggerLifecycleTestScope scope;
            InputSystem& input = scope.GetInput();

            InputAction fire("IA_FireValue", InputActionValueType::Boolean);
            fire.AddTrigger(std::make_shared<InputTriggerPressed>());

            InputMappingContext context({{&fire, InputKeys::Key_Space}});
            input.AddInputMappingContext(&context, 0);

            PressKey(input, InputKeys::Key_Space);
            TickFrame(input);
            if (ValueIsZero(input, &fire))
            {
                ME_LOG(LogTest, Error, "Released value: press should produce a non-zero value.");
                return false;
            }

            ReleaseKey(input, InputKeys::Key_Space);
            TickFrame(input);
            if (!ValueIsZero(input, &fire))
            {
                ME_LOG(LogTest, Error, "Released value: value must be zero after release.");
                return false;
            }

            TickFrame(input);
            if (!ValueIsZero(input, &fire))
            {
                ME_LOG(LogTest, Error, "Released value: idle frame must keep the value at zero.");
                return false;
            }

            return true;
        }

        // Down keeps its meaning: a single activation on press, Ongoing for every held frame.
        bool RunDownHoldStaysOngoing()
        {
            InputTriggerLifecycleTestScope scope;
            InputSystem& input = scope.GetInput();

            InputAction sneak("IA_SneakLifecycle", InputActionValueType::Boolean);
            sneak.AddTrigger(std::make_shared<InputTriggerDown>());

            InputMappingContext context({{&sneak, InputKeys::Key_LeftControl}});
            input.AddInputMappingContext(&context, 0);

            PressKey(input, InputKeys::Key_LeftControl);
            TickFrame(input);
            if (EventOf(input, &sneak) != InputTriggerEvent::Started)
            {
                ME_LOG(LogTest, Error, "Down hold: press should emit Started.");
                return false;
            }

            int activationCount = 1;
            for (int frame = 0; frame < 3; ++frame)
            {
                HoldKey(input, InputKeys::Key_LeftControl);
                TickFrame(input);
                if (EventOf(input, &sneak) == InputTriggerEvent::Started
                    || EventOf(input, &sneak) == InputTriggerEvent::Triggered)
                {
                    ++activationCount;
                }
                if (StateOf(input, &sneak) != InputTriggerState::Ongoing)
                {
                    ME_LOG(LogTest, Error, "Down hold: frame {0} should stay Ongoing.", frame);
                    return false;
                }
            }

            ReleaseKey(input, InputKeys::Key_LeftControl);
            TickFrame(input);
            if (EventOf(input, &sneak) != InputTriggerEvent::Canceled)
            {
                ME_LOG(LogTest, Error, "Down hold: release should emit Canceled.");
                return false;
            }

            if (activationCount != 1)
            {
                ME_LOG(LogTest, Error, "Down hold: expected a single activation, saw {0}.", activationCount);
                return false;
            }

            return true;
        }

        // A chord gates its consumer: while the chord action is inactive the consumer cannot fire,
        // and while it is held the consumer sees that state in the very frame it is pressed.
        bool RunChordGatesConsumer()
        {
            InputTriggerLifecycleTestScope scope;
            InputSystem& input = scope.GetInput();

            // A held aim reports the level state, so it is Triggered for as long as it is held.
            InputAction aim("IA_AimChord", InputActionValueType::Boolean);

            InputAction fire("IA_FireChord", InputActionValueType::Boolean);
            std::shared_ptr<InputTriggerChordAction> chordTrigger = std::make_shared<InputTriggerChordAction>();
            chordTrigger->ChordAction = &aim;
            fire.AddTrigger(chordTrigger);

            InputMappingContext context({{&aim, InputKeys::Key_RightShift}, {&fire, InputKeys::Key_Space}});
            input.AddInputMappingContext(&context, 0);

            PressKey(input, InputKeys::Key_Space);
            TickFrame(input);
            if (EventOf(input, &fire) == InputTriggerEvent::Triggered)
            {
                ME_LOG(LogTest, Error, "Chord: consumer must not fire while the chord action is idle.");
                return false;
            }

            ReleaseKey(input, InputKeys::Key_Space);
            TickFrame(input);

            // Both keys go down in the same frame: the consumer can only see the chord action as
            // active if that action is evaluated first.
            PressKey(input, InputKeys::Key_RightShift);
            PressKey(input, InputKeys::Key_Space);
            TickFrame(input);
            if (EventOf(input, &fire) != InputTriggerEvent::Triggered)
            {
                ME_LOG(LogTest, Error, "Chord: consumer must fire in the same frame its chord action activates.");
                return false;
            }

            // Holding the chord action keeps the gate open for a new press.
            ReleaseKey(input, InputKeys::Key_Space);
            TickFrame(input);
            HoldKey(input, InputKeys::Key_RightShift);
            TickFrame(input);
            PressKey(input, InputKeys::Key_Space);
            TickFrame(input);
            if (EventOf(input, &fire) != InputTriggerEvent::Triggered)
            {
                ME_LOG(LogTest, Error, "Chord: consumer must fire again while the chord action stays held.");
                return false;
            }

            // Releasing the chord action closes the gate again.
            ReleaseKey(input, InputKeys::Key_Space);
            TickFrame(input);
            ReleaseKey(input, InputKeys::Key_RightShift);
            TickFrame(input);
            PressKey(input, InputKeys::Key_Space);
            TickFrame(input);
            if (EventOf(input, &fire) == InputTriggerEvent::Triggered)
            {
                ME_LOG(LogTest, Error, "Chord: consumer must stay closed after the chord action was released.");
                return false;
            }

            return true;
        }

        // A cycle has no evaluation order; it must be reported rather than looping or being cut short.
        bool RunChordCycleReportsError()
        {
            InputTriggerLifecycleTestScope scope;
            InputSystem& input = scope.GetInput();

            InputAction first("IA_CycleFirst", InputActionValueType::Boolean);
            InputAction second("IA_CycleSecond", InputActionValueType::Boolean);

            std::shared_ptr<InputTriggerChordAction> firstNeedsSecond = std::make_shared<InputTriggerChordAction>();
            firstNeedsSecond->ChordAction = &second;
            first.AddTrigger(firstNeedsSecond);

            std::shared_ptr<InputTriggerChordAction> secondNeedsFirst = std::make_shared<InputTriggerChordAction>();
            secondNeedsFirst->ChordAction = &first;
            second.AddTrigger(secondNeedsFirst);

            InputMappingContext context({{&first, InputKeys::Key_LeftShift}, {&second, InputKeys::Key_Space}});
            input.AddInputMappingContext(&context, 0);

            PressKey(input, InputKeys::Key_LeftShift);
            TickFrame(input);
            if (!Testing::TestAccess<InputSystem>::HasChordCycleError(input))
            {
                ME_LOG(LogTest, Error, "Chord cycle: the dependency cycle must be reported.");
                return false;
            }

            PressKey(input, InputKeys::Key_Space);
            TickFrame(input);
            if (!Testing::TestAccess<InputSystem>::HasChordCycleError(input))
            {
                ME_LOG(LogTest, Error, "Chord cycle: the cycle must still be reported on a later frame.");
                return false;
            }

            return true;
        }
    }

    bool RunInputTriggerLifecycleTests()
    {
        return RunPressedCycles()
            && RunPressedHoldFiresOnce()
            && RunValueFallsToZeroOnRelease()
            && RunDownHoldStaysOngoing()
            && RunChordGatesConsumer()
            && RunChordCycleReportsError();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("input-trigger-lifecycle: press release re-arm, value reset and chord order [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunInputTriggerLifecycleTests());
}
