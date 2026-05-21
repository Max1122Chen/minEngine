#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Input/InputAction.h"

namespace minEngine
{
    struct InputActionEventBinding
    {
        InputAction* Action;
        InputTriggerEvent TriggerEvent;
        std::function<void(const InputActionValue&)> Callback;
    };

    ME_CLASS()
    class InputComponent : public Component
    {
        ME_GENERATED_BODY(InputComponent)
        using InputActionCallback =  std::function<void(const InputActionValue&)>;

    public:
        InputComponent() = default;
        virtual ~InputComponent() override
        {
            InputSystem::Get().RemoveInputComponent(this);
        }

        void RegisterInputComponent()
        {
            InputSystem::Get().AddInputComponent(this);
        }


        void ProcessInputAction(const InputAction* action, InputTriggerEvent triggerEvent, const InputActionValue& value)
        {
            for(const auto& binding : m_ActionEventBindings)
            {
                if(binding.Action == action && binding.TriggerEvent == triggerEvent)
                {
                    binding.Callback(value);
                }
            }
        }


        void BindAction(InputAction* action, InputTriggerEvent triggerEvent, InputActionCallback callback)
        {
            m_ActionEventBindings.push_back({action, triggerEvent, callback});
        }

    private:
        std::vector<InputActionEventBinding> m_ActionEventBindings;
    };
}

#include "Generated/Reflection/InputComponent.gen.h"