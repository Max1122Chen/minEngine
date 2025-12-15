#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "InputTriggers.h"
#include "InputModifiers.h"

namespace minEngine
{
    enum class InputActionValueType;
    class InputActionValue;

    class InputAction
    {
    public:
        InputAction(const std::string& name, InputActionValueType valueType, bool bConsumeInput = true)
            : m_Name(name), m_ValueType(valueType), m_bConsumeInput(bConsumeInput)
        {}
        ~InputAction() = default;

        const std::string& GetName() const { return m_Name; }
        InputActionValueType GetValueType() const { return m_ValueType; }
        bool DoesConsumeInput() const { return m_bConsumeInput; }

        std::vector<std::shared_ptr<InputTrigger>>& GetTriggers() { return m_Triggers; }
        void AddTrigger(std::shared_ptr<InputTrigger> trigger)
        {
            m_Triggers.push_back(trigger);
        }

        std::vector<std::shared_ptr<InputModifier>>& GetModifiers() { return m_Modifiers; }
        void AddModifier(std::shared_ptr<InputModifier> modifier)
        {
            m_Modifiers.push_back(modifier);
        }

    private:
        std::string m_Name;
        InputActionValueType m_ValueType;
        bool m_bConsumeInput = true;

        std::vector<std::shared_ptr<InputTrigger>> m_Triggers;

        // IA's own modifiers, which will be applied to all its "Instances"
        std::vector<std::shared_ptr<InputModifier>> m_Modifiers;
    };

    struct InputTriggerStateTracker
    {
        void SetNoTriggerState(InputTriggerState inState)
        {
            NoTriggerState = inState;
        }
        InputTriggerState GetState() const;
        InputTriggerState EvaluateTriggerState(const std::vector<std::shared_ptr<InputTrigger>>& triggers, const InputActionValue& modifiedValue, float deltaTime);
    
        bool operator>=(const InputTriggerStateTracker& rhs) const { return GetState() >= rhs.GetState(); }
        bool operator<(const InputTriggerStateTracker& rhs) const { return GetState() < rhs.GetState(); }

    private:
        InputTriggerState NoTriggerState = InputTriggerState::None;

        bool bEvaluatedTriggers = false;
        bool bFoundActiveTrigger = false;
        bool bFoundExplicit = false;
        bool bAnyExplicitTriggered = false;
        bool bAllImplicitTriggered = false;
        bool bBlocking = false;
    }; 

    struct InputActionInstance
    {
        InputAction* SourceAction = nullptr;

        InputTriggerStateTracker TriggerStateTracker;
        InputTriggerState LastState = InputTriggerState::None;

        InputTriggerEvent TriggerEvent;

        InputActionValue Value;
    };
}