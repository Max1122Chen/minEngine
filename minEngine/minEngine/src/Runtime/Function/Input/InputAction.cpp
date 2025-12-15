#include "InputAction.h"

namespace minEngine
{
    InputTriggerState InputTriggerStateTracker::GetState() const
    {
        if(!bEvaluatedTriggers)
        {
            return NoTriggerState;
        }

        if(bBlocking)
        {
            return InputTriggerState::None;
        }

        bool bTriggered = (!bFoundExplicit || bAnyExplicitTriggered) && bAllImplicitTriggered;
        return bTriggered ? InputTriggerState::Triggered : (bFoundActiveTrigger ? InputTriggerState::Ongoing : InputTriggerState::None);
    }

    InputTriggerState InputTriggerStateTracker::EvaluateTriggerState(const std::vector<std::shared_ptr<InputTrigger>> &triggers, const InputActionValue &modifiedValue, float deltaTime)
    {
        for(auto& trigger : triggers)
        {
            if(!trigger)
            {
                continue;
            }

            bEvaluatedTriggers = true;
            InputTriggerState currentState = trigger->UpdateTriggerState(modifiedValue, deltaTime);

            trigger->lastValue = modifiedValue;
            trigger->m_LastState = currentState;

            switch(trigger->GetTriggerType())
            {
            case InputTriggerType::Explicit:
                bFoundExplicit = true;
                bAnyExplicitTriggered |= (currentState == InputTriggerState::Triggered);
                bFoundActiveTrigger |= (currentState != InputTriggerState::None);
                break;
            case InputTriggerType::Implicit:
                bAllImplicitTriggered &= (currentState == InputTriggerState::Triggered);
                bFoundActiveTrigger |= (currentState != InputTriggerState::None);
                break;
            case InputTriggerType::Blocker:
                break;
            }

            bBlocking |= trigger->IsBlocking(currentState);
        }
        return GetState();
    }
}