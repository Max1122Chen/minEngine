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
    class InputTrigger
    {
    public:
        InputTrigger() = default;
        virtual ~InputTrigger() = default;

        float m_ActuationThreshold = 0.5f;

        InputTriggerState m_TriggerState = InputTriggerState::None;
        InputTriggerState m_LastState = InputTriggerState::None;
        InputActionValue lastValue;

        // Return Explicit as default
        virtual InputTriggerType GetTriggerType() const { return InputTriggerType::Explicit; }

        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) = 0;
        bool IsActuated(const InputActionValue& modifiedValue) { return modifiedValue.GetMagnitudeSq() > m_ActuationThreshold * m_ActuationThreshold; }
        
        // Return false as default
        virtual bool IsBlocking(InputTriggerState triggerState) { return false; }
    };

    class InputTriggerTimeBased //: public InputTrigger
    {
        
    };

    // Simple trigger that fires while the input is down (actuated). Down = Pressed or held.
    class InputTriggerDown : public InputTrigger
    {
    public:
        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) override;
    };

    // Simple trigger that fires when the input is first pressed (actuated).
    class InputTriggerPressed : public InputTrigger
    {
    public:
        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) override;
    };

    // Simple trigger that fires when the input is released (de-actuated).
    class InputTriggerReleased : public InputTrigger
    {
    public:
        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) override;
    };

    class InputTriggerChordAction : public InputTrigger
    {
    public:
        InputAction* ChordAction = nullptr;

        virtual InputTriggerType GetTriggerType() const override { return InputTriggerType::Implicit; }

        virtual InputTriggerState UpdateTriggerState(const InputActionValue& modifiedValue, float deltaTime) override;
    };
}