#pragma once
#include "Core.h"
#include "InputActionValue.h"

namespace minEngine
{
    class InputAction;
    class InputActionValue;

    enum class InputTriggerType : uint8_t
    {
        Explicit,
        Implicit,
        Blocker
    };

    enum class InputTriggerState : uint8_t
    {
        None,
        Ongoing,
        Triggered,
    };

    enum class InputTriggerEvent : uint8_t
    {
        None,
        Started,
        Triggered,
        Ongoing,
        Canceled,
        Completed
    };

    /**
     * @brief 
     * Owned by InputAction or InputActionInstance.  Defines when an InputAction should be triggered based on the modified input value.
     */
    class MINENGINE_API InputTrigger
    {
    public:
        InputTrigger() = default;
        virtual ~InputTrigger() = default;

        float m_ActuationThreshold = 0.5f;

        InputTriggerState m_TriggerState = InputTriggerState::None;
        InputActionValue lastValue;

        // Return Explicit as default
        virtual InputTriggerType GetTriggerType() const { return InputTriggerType::Explicit; }

        // Action this trigger depends on, or nullptr when it depends on nothing. The input system
        // uses it to evaluate dependencies before their consumers, so a chord reads this frame.
        virtual InputAction* GetChordAction() const { return nullptr; }

        // The input system pushes the dependency's state in before evaluating the trigger, so a
        // trigger never reaches back into the input system for it.
        virtual void SetChordDependencyState(InputTriggerState state) { (void)state; }

        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) = 0;
        bool IsActuated(const InputActionValue& modifiedValue) { return modifiedValue.GetMagnitudeSq() > m_ActuationThreshold * m_ActuationThreshold; }
        
        // Return false as default
        virtual bool IsBlocking(InputTriggerState triggerState) { return false; }
    };

    class InputTriggerTimeBased //: public InputTrigger
    {
        
    };

    // Simple trigger that fires while the input is down (actuated). Down = Pressed or held.
    class MINENGINE_API InputTriggerDown : public InputTrigger
    {
    public:
        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) override;
    };

    // Simple trigger that fires when the input is first pressed (actuated).
    class MINENGINE_API InputTriggerPressed : public InputTrigger
    {
    public:
        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) override;
    };

    // Simple trigger that fires when the input is released (de-actuated).
    class MINENGINE_API InputTriggerReleased : public InputTrigger
    {
    public:
        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) override;
    };

    class MINENGINE_API InputTriggerChordAction : public InputTrigger
    {
    public:
        InputAction* ChordAction = nullptr;

        virtual InputTriggerType GetTriggerType() const override { return InputTriggerType::Implicit; }
        virtual InputAction* GetChordAction() const override { return ChordAction; }

        virtual void SetChordDependencyState(InputTriggerState state) override
        {
            ChordDependencyState = state;
        }

        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) override
        {
            (void)modifiedValue;
            (void)deltaTime;

            // The dependency is evaluated before its consumers, so this is the state the chord
            // action reached this frame; an inactive chord action reports None and blocks.
            return ChordAction != nullptr ? ChordDependencyState : InputTriggerState::None;
        }

    private:
        InputTriggerState ChordDependencyState = InputTriggerState::None;
    };
}