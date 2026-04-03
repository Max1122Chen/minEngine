#pragma once
#include "Core.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "InputKeys.h"
#include "InputKeyTypes.h"
#include "InputKeyState.h"
#include "InputAction.h"


namespace minEngine
{
    class RuntimeGlobalContext;

    class InputComponent;
    class InputMappingContext;

    struct ActiveInputMappingContext
    {
        InputMappingContext* Context;
        int Priority = 0;
    };

    /**
     * @brief 
     * A simple input system that handles input events, manages input mappings, and processes input actions.
     * Inspired by Unreal Engine's Enhanced Input System.
     * Implemented Feature List:
     * - Input Mapping Context ( IMC ). With InputKeyActionMapping.
     * - Input Actions ( IA ). With InputTrigger and InputModifier.
     */
    class InputSystem       
    {
    // TODO: To support multiple players later, we may need to have PlayerInputSystem or LocalInputSystem
    public:
        InputSystem() = default;
        ~InputSystem() = default;

        void Initialize();
        void Shutdown();
        static InputSystem& GetInputSystem() { return *RuntimeGlobalContext::GetRuntimeGlobalContext().m_InputSystem; }

        void Tick(float deltaTime);

        void AddInputComponent(InputComponent* component);
        void RemoveInputComponent(InputComponent* component);


        // Input Mapping Context Management. 
        // Maybe move it to something like "PlayerLocalInputSystem" later
        void AddInputMappingContext(InputMappingContext* context, int priority = 0);
        void RemoveInputMappingContext(InputMappingContext* context);

        InputActionInstance* FindInputActionInstance(InputAction* action);

        void OnKey(InputKey key, int scancode, InputKeyAction action, int mods);
        void OnCursorPos(double xPos, double yPos);
        void OnMouseScroll(double xOffset, double yOffset);


    private:
        InputKeyState* GetKeyState(const InputKey& key) { return &m_KeyStateMap[key]; }
        const InputKeyState* GetKeyState(const InputKey& key) const { return &m_KeyStateMap.at(key); }
        


        InputActionValue ApplyModifiers(const std::vector<std::shared_ptr<InputModifier>>& modifiers, const InputActionValue& rawValue, float deltaTime);

        InputTriggerEvent GetTriggerStateChangeEvent(InputTriggerState lastState, InputTriggerState newState);

    private:
        
        InputKeys m_InputKeys;

        std::unordered_map<InputKey, InputKeyState, InputKey::Hash> m_KeyStateMap;

        std::shared_ptr<InputMappingContext> m_DefaultContext;

        std::vector<ActiveInputMappingContext> m_ActiveContexts;

        std::unordered_map<InputAction*, InputActionInstance> m_ActionInstances;
        std::vector<InputAction*> m_ActionsWithEventThisTick;

        // Registered Input Components
        std::vector<InputComponent*> m_InputComponents;
    };
}