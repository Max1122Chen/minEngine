#pragma once
#include "Core.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "InputKeys.h"
#include "InputKeyState.h"
#include "InputAction.h"

#include "glfw/glfw3.h" // For GLFW key codes. TODO: remove dependency later

namespace minEngine
{
    class RuntimeGlobalContext;

    class InputActionKeyMapping;
    class InputMappingContext;
    class InputComponent;

    struct ActiveInputMappingContext
    {
        InputMappingContext* Context;
        int Priority = 0;
    };

    // TODO: To support multiple players later, we may need to have PlayerInputSystem or LocalInputSystem
    class InputSystem
    {
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

        void OnKey(int key, int scancode, int action, int mods);
        void OnCursorPos(double xPos, double yPos);


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

        float m_CursorSensitivity = 0.01f;

        float m_LastCursorX = 0;
        float m_LastCursorY = 0;
        
    };
}