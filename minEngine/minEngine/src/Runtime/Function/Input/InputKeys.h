#pragma once
#include "Core.h"
#include "InputKeyTypes.h"

namespace minEngine
{
    /**
     * Abstract input key identity (name + axis kind).
     * Device backends map platform codes into these keys.
     */
    struct InputKey
    {
        std::string Name;
        InputAxisType AxisType = InputAxisType::None;

        InputKey() = default;
        InputKey(const std::string& inName, InputAxisType inAxisType)
            : Name(inName)
            , AxisType(inAxisType)
        {
        }

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

        // Mouse
        static const InputKey MouseX;
        static const InputKey MouseY;
        static const InputKey Mouse2D;
        static const InputKey MouseScroll;
        static const InputKey Mouse_Left;
        static const InputKey Mouse_Middle;
        static const InputKey Mouse_Right;
        static const InputKey Mouse_4;
        static const InputKey Mouse_5;
        static const InputKey Mouse_6;
        static const InputKey Mouse_7;
        static const InputKey Mouse_8;

        // Alphabet
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

        // Top-row digits
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

        // Function
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
        static const InputKey Key_F13;
        static const InputKey Key_F14;
        static const InputKey Key_F15;
        static const InputKey Key_F16;
        static const InputKey Key_F17;
        static const InputKey Key_F18;
        static const InputKey Key_F19;
        static const InputKey Key_F20;
        static const InputKey Key_F21;
        static const InputKey Key_F22;
        static const InputKey Key_F23;
        static const InputKey Key_F24;

        // Arrows / navigation
        static const InputKey Key_Up;
        static const InputKey Key_Down;
        static const InputKey Key_Left;
        static const InputKey Key_Right;
        static const InputKey Key_Insert;
        static const InputKey Key_Delete;
        static const InputKey Key_Home;
        static const InputKey Key_End;
        static const InputKey Key_PageUp;
        static const InputKey Key_PageDown;

        // Printable punctuation (US layout names)
        static const InputKey Key_Space;
        static const InputKey Key_Apostrophe;
        static const InputKey Key_Comma;
        static const InputKey Key_Minus;
        static const InputKey Key_Period;
        static const InputKey Key_Slash;
        static const InputKey Key_Semicolon;
        static const InputKey Key_Equal;
        static const InputKey Key_LeftBracket;
        static const InputKey Key_Backslash;
        static const InputKey Key_RightBracket;
        static const InputKey Key_GraveAccent;
        static const InputKey Key_World1;
        static const InputKey Key_World2;

        // Editing / locks / system
        static const InputKey Key_Enter;
        static const InputKey Key_Escape;
        static const InputKey Key_Tab;
        static const InputKey Key_Backspace;
        static const InputKey Key_CapsLock;
        static const InputKey Key_ScrollLock;
        static const InputKey Key_NumLock;
        static const InputKey Key_PrintScreen;
        static const InputKey Key_Pause;
        static const InputKey Key_Menu;

        // Modifiers — sides + synthetic (any side)
        static const InputKey Key_Shift;
        static const InputKey Key_Control;
        static const InputKey Key_Alt;
        static const InputKey Key_Super;
        static const InputKey Key_LeftShift;
        static const InputKey Key_RightShift;
        static const InputKey Key_LeftControl;
        static const InputKey Key_RightControl;
        static const InputKey Key_LeftAlt;
        static const InputKey Key_RightAlt;
        static const InputKey Key_LeftSuper;
        static const InputKey Key_RightSuper;

        // Numpad
        static const InputKey Key_KP_0;
        static const InputKey Key_KP_1;
        static const InputKey Key_KP_2;
        static const InputKey Key_KP_3;
        static const InputKey Key_KP_4;
        static const InputKey Key_KP_5;
        static const InputKey Key_KP_6;
        static const InputKey Key_KP_7;
        static const InputKey Key_KP_8;
        static const InputKey Key_KP_9;
        static const InputKey Key_KP_Decimal;
        static const InputKey Key_KP_Divide;
        static const InputKey Key_KP_Multiply;
        static const InputKey Key_KP_Subtract;
        static const InputKey Key_KP_Add;
        static const InputKey Key_KP_Enter;
        static const InputKey Key_KP_Equal;

        // Gamepad (Xbox-layout canonical names; PS face aliases share the same Name)
        static const InputKey Gamepad_A;
        static const InputKey Gamepad_B;
        static const InputKey Gamepad_X;
        static const InputKey Gamepad_Y;
        static const InputKey Gamepad_Cross;     // alias Name == Gamepad_A
        static const InputKey Gamepad_Circle;    // alias Name == Gamepad_B
        static const InputKey Gamepad_Square;    // alias Name == Gamepad_X
        static const InputKey Gamepad_Triangle;  // alias Name == Gamepad_Y
        static const InputKey Gamepad_LeftBumper;
        static const InputKey Gamepad_RightBumper;
        static const InputKey Gamepad_Back;
        static const InputKey Gamepad_Start;
        static const InputKey Gamepad_Guide;
        static const InputKey Gamepad_LeftThumb;
        static const InputKey Gamepad_RightThumb;
        static const InputKey Gamepad_DPad_Up;
        static const InputKey Gamepad_DPad_Right;
        static const InputKey Gamepad_DPad_Down;
        static const InputKey Gamepad_DPad_Left;
        static const InputKey Gamepad_LeftX;
        static const InputKey Gamepad_LeftY;
        static const InputKey Gamepad_RightX;
        static const InputKey Gamepad_RightY;
        static const InputKey Gamepad_LeftTrigger;
        static const InputKey Gamepad_RightTrigger;

    public:
        void Initialize();
        void AddKey(const InputKey& key);

        const std::vector<InputKey>& GetAllKeys() const { return m_Keys; }

    private:
        std::vector<InputKey> m_Keys;
    };
}
