#pragma once
#include "Core.h"

namespace minEngine
{

    class InputSystem
    {
    public:
        InputSystem() = default;
        ~InputSystem() = default;

        void Initialize();
        void Shutdown();

        void OnKey(int key, int scancode, int action, int mods);
        void OnCursorPos(double xPos, double yPos);


    private:
        float m_CursorSensitivity = 0.01f;

        float m_LastCursorX = 0;
        float m_LastCursorY = 0;
        
    };
}