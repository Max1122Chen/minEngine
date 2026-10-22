#include "InputTriggers.h"

namespace minEngine
{
    InputTriggerState InputTriggerDown::UpdateTriggerState(const InputActionValue &modifiedValue, float deltaTime)
    {
        return IsActuated(modifiedValue) ? InputTriggerState::Ongoing : InputTriggerState::None;
    }

    InputTriggerState InputTriggerPressed::UpdateTriggerState(const InputActionValue &modifiedValue, float deltaTime)
    {
        return (IsActuated(modifiedValue) && !IsActuated(lastValue)) ? InputTriggerState::Triggered : InputTriggerState::None;
    }

    InputTriggerState InputTriggerReleased::UpdateTriggerState(const InputActionValue &modifiedValue, float deltaTime)
    {
        if(IsActuated(modifiedValue))
        {
            return InputTriggerState::Ongoing;
        }
        
        if(IsActuated(lastValue))
        {
            return InputTriggerState::Triggered;
        }

        return InputTriggerState::None;
    }
}