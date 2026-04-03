#pragma once
#include "Core.h"
#include "InputKeyTypes.h"

namespace minEngine
{
    
    /**
     * @brief 
     * minEngine predefined input keys.
     * Help to abstract different input devices and APIs.
     */
    struct InputKey
    {
        std::string Name;
        InputAxisType AxisType = InputAxisType::None;

        InputKey() = default;
        InputKey(const std::string& inName, InputAxisType inAxisType) : Name(inName), AxisType(inAxisType) {}

        friend bool operator==(const InputKey& lhs, const InputKey& rhs) { return lhs.Name == rhs.Name; }
        friend bool operator!=(const InputKey& lhs, const InputKey& rhs) { return !(lhs == rhs); }

        struct Hash
        {
            std::size_t operator()(const InputKey& key) const
            {
                return std::hash<std::string>()(key.Name);
            }
        };



    };

    struct InputKeys
    {
        static const InputKey AnyKey;

    // Mouse keys
        static const InputKey MouseX;
        static const InputKey MouseY;
        static const InputKey Mouse2D;
        static const InputKey MouseScroll;
        static const InputKey Mouse_Left;
        static const InputKey Mouse_Middle;
        static const InputKey Mouse_Right;

    // Keyboard keys
        // Alphabet keys
        static const InputKey Key_A;
        static const InputKey Key_B;
        static const InputKey Key_C;
        static const InputKey Key_D;
        static const InputKey Key_E;
        static const InputKey Key_F;
        static const InputKey Key_G;
        static const InputKey Key_H;
        static const InputKey Key_I;
        static const InputKey Key_J;
        static const InputKey Key_K;
        static const InputKey Key_L;
        static const InputKey Key_M;
        static const InputKey Key_N;
        static const InputKey Key_O;
        static const InputKey Key_P;
        static const InputKey Key_Q;
        static const InputKey Key_R;
        static const InputKey Key_S;
        static const InputKey Key_T;
        static const InputKey Key_U;
        static const InputKey Key_V;
        static const InputKey Key_W;
        static const InputKey Key_X;
        static const InputKey Key_Y;
        static const InputKey Key_Z;
        
        // Number keys
        static const InputKey Key_0;
        static const InputKey Key_1;
        static const InputKey Key_2;
        static const InputKey Key_3;
        static const InputKey Key_4;
        static const InputKey Key_5;
        static const InputKey Key_6;
        static const InputKey Key_7;
        static const InputKey Key_8;
        static const InputKey Key_9;
        
        // Function keys
        static const InputKey Key_F1;
        static const InputKey Key_F2;
        static const InputKey Key_F3;
        static const InputKey Key_F4;
        static const InputKey Key_F5;
        static const InputKey Key_F6;
        static const InputKey Key_F7;
        static const InputKey Key_F8;
        static const InputKey Key_F9;
        static const InputKey Key_F10;
        static const InputKey Key_F11;
        static const InputKey Key_F12;
        
        // Arrow keys
        static const InputKey Key_Up;
        static const InputKey Key_Down;
        static const InputKey Key_Left;
        static const InputKey Key_Right;
        
        // Other common keys
        static const InputKey Key_Space;
        static const InputKey Key_Enter;
        static const InputKey Key_Escape;
        static const InputKey Key_Tab;
        static const InputKey Key_Backspace;
        static const InputKey Key_Shift;
        static const InputKey Key_Control;
        static const InputKey Key_Alt;
        static const InputKey Key_CapsLock;
        static const InputKey Key_LeftShift;
        static const InputKey Key_RightShift;
        static const InputKey Key_LeftControl;
        static const InputKey Key_RightControl;
        static const InputKey Key_LeftAlt;
        static const InputKey Key_RightAlt;

    public:
        void Initialize();
        void AddKey(const InputKey& key);

        const std::vector<InputKey>& GetAllKeys() const { return m_Keys; }

    
    private:
        std::vector<InputKey> m_Keys;

    };
}