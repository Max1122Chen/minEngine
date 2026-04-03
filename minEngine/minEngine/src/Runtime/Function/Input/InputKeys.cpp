#include "InputKeys.h"
#include "glfw/glfw3.h" // For GLFW key codes. TODO: remove dependency later

namespace minEngine
{

    const InputKey InputKeys::AnyKey("AnyKey", InputAxisType::None);
    // Mouse keys
    const InputKey InputKeys::MouseX("MouseX", InputAxisType::Axis1D);
    const InputKey InputKeys::MouseY("MouseY", InputAxisType::Axis1D);
    const InputKey InputKeys::Mouse2D("Mouse2D", InputAxisType::Axis2D);
    const InputKey InputKeys::MouseScroll("MouseScroll", InputAxisType::Axis1D);
    const InputKey InputKeys::Mouse_Left("Mouse_Left", InputAxisType::Button);
    const InputKey InputKeys::Mouse_Middle("Mouse_Middle", InputAxisType::Button);
    const InputKey InputKeys::Mouse_Right("Mouse_Right", InputAxisType::Button);


    // Keyboard keys
    // Alphabet keys
    const InputKey InputKeys::Key_A("Key_A", InputAxisType::Button);
    const InputKey InputKeys::Key_B("Key_B", InputAxisType::Button);
    const InputKey InputKeys::Key_C("Key_C", InputAxisType::Button);
    const InputKey InputKeys::Key_D("Key_D", InputAxisType::Button);
    const InputKey InputKeys::Key_E("Key_E", InputAxisType::Button);
    const InputKey InputKeys::Key_F("Key_F", InputAxisType::Button);
    const InputKey InputKeys::Key_G("Key_G", InputAxisType::Button);
    const InputKey InputKeys::Key_H("Key_H", InputAxisType::Button);
    const InputKey InputKeys::Key_I("Key_I", InputAxisType::Button);
    const InputKey InputKeys::Key_J("Key_J", InputAxisType::Button);
    const InputKey InputKeys::Key_K("Key_K", InputAxisType::Button);
    const InputKey InputKeys::Key_L("Key_L", InputAxisType::Button);
    const InputKey InputKeys::Key_M("Key_M", InputAxisType::Button);
    const InputKey InputKeys::Key_N("Key_N", InputAxisType::Button);
    const InputKey InputKeys::Key_O("Key_O", InputAxisType::Button);
    const InputKey InputKeys::Key_P("Key_P", InputAxisType::Button);
    const InputKey InputKeys::Key_Q("Key_Q", InputAxisType::Button);
    const InputKey InputKeys::Key_R("Key_R", InputAxisType::Button);
    const InputKey InputKeys::Key_S("Key_S", InputAxisType::Button);
    const InputKey InputKeys::Key_T("Key_T", InputAxisType::Button);
    const InputKey InputKeys::Key_U("Key_U", InputAxisType::Button);
    const InputKey InputKeys::Key_V("Key_V", InputAxisType::Button);
    const InputKey InputKeys::Key_W("Key_W", InputAxisType::Button);
    const InputKey InputKeys::Key_X("Key_X", InputAxisType::Button);
    const InputKey InputKeys::Key_Y("Key_Y", InputAxisType::Button);
    const InputKey InputKeys::Key_Z("Key_Z", InputAxisType::Button);
    
    // Number keys
    const InputKey InputKeys::Key_0("Key_0", InputAxisType::Button);
    const InputKey InputKeys::Key_1("Key_1", InputAxisType::Button);
    const InputKey InputKeys::Key_2("Key_2", InputAxisType::Button);
    const InputKey InputKeys::Key_3("Key_3", InputAxisType::Button);
    const InputKey InputKeys::Key_4("Key_4", InputAxisType::Button);
    const InputKey InputKeys::Key_5("Key_5", InputAxisType::Button);
    const InputKey InputKeys::Key_6("Key_6", InputAxisType::Button);
    const InputKey InputKeys::Key_7("Key_7", InputAxisType::Button);
    const InputKey InputKeys::Key_8("Key_8", InputAxisType::Button);
    const InputKey InputKeys::Key_9("Key_9", InputAxisType::Button);
    
    // Function keys
    const InputKey InputKeys::Key_F1("Key_F1", InputAxisType::Button);
    const InputKey InputKeys::Key_F2("Key_F2", InputAxisType::Button);
    const InputKey InputKeys::Key_F3("Key_F3", InputAxisType::Button);
    const InputKey InputKeys::Key_F4("Key_F4", InputAxisType::Button);
    const InputKey InputKeys::Key_F5("Key_F5", InputAxisType::Button);
    const InputKey InputKeys::Key_F6("Key_F6", InputAxisType::Button);
    const InputKey InputKeys::Key_F7("Key_F7", InputAxisType::Button);
    const InputKey InputKeys::Key_F8("Key_F8", InputAxisType::Button);
    const InputKey InputKeys::Key_F9("Key_F9", InputAxisType::Button);
    const InputKey InputKeys::Key_F10("Key_F10", InputAxisType::Button);
    const InputKey InputKeys::Key_F11("Key_F11", InputAxisType::Button);
    const InputKey InputKeys::Key_F12("Key_F12", InputAxisType::Button);
    
    // Arrow keys
    const InputKey InputKeys::Key_Up("Key_Up", InputAxisType::Button);
    const InputKey InputKeys::Key_Down("Key_Down", InputAxisType::Button);
    const InputKey InputKeys::Key_Left("Key_Left", InputAxisType::Button);
    const InputKey InputKeys::Key_Right("Key_Right", InputAxisType::Button);
    
    // Other common keys
    const InputKey InputKeys::Key_Space("Key_Space", InputAxisType::Button);
    const InputKey InputKeys::Key_Enter("Key_Enter", InputAxisType::Button);
    const InputKey InputKeys::Key_Escape("Key_Escape", InputAxisType::Button);
    const InputKey InputKeys::Key_Tab("Key_Tab", InputAxisType::Button);
    const InputKey InputKeys::Key_Backspace("Key_Backspace", InputAxisType::Button);
    const InputKey InputKeys::Key_Shift("Key_Shift", InputAxisType::Button);
    const InputKey InputKeys::Key_Control("Key_Control", InputAxisType::Button);
    const InputKey InputKeys::Key_Alt("Key_Alt", InputAxisType::Button);
    const InputKey InputKeys::Key_CapsLock("Key_CapsLock", InputAxisType::Button);
    const InputKey InputKeys::Key_LeftShift("Key_LeftShift", InputAxisType::Button);
    const InputKey InputKeys::Key_RightShift("Key_RightShift", InputAxisType::Button);
    const InputKey InputKeys::Key_LeftControl("Key_LeftControl", InputAxisType::Button);
    const InputKey InputKeys::Key_RightControl("Key_RightControl", InputAxisType::Button);
    const InputKey InputKeys::Key_LeftAlt("Key_LeftAlt", InputAxisType::Button);
    const InputKey InputKeys::Key_RightAlt("Key_RightAlt", InputAxisType::Button);


    void InputKeys::Initialize()
    {
        AddKey(AnyKey);

        AddKey(MouseX);
        AddKey(MouseY);
        AddKey(Mouse2D);
        AddKey(MouseScroll);
        AddKey(Mouse_Left);
        AddKey(Mouse_Middle);
        AddKey(Mouse_Right);

        AddKey(Key_A);
        AddKey(Key_B);
        AddKey(Key_C);
        AddKey(Key_D);
        AddKey(Key_E);
        AddKey(Key_F);
        AddKey(Key_G);
        AddKey(Key_H);
        AddKey(Key_I);
        AddKey(Key_J);
        AddKey(Key_K);
        AddKey(Key_L);
        AddKey(Key_M);
        AddKey(Key_N);
        AddKey(Key_O);
        AddKey(Key_P);
        AddKey(Key_Q);
        AddKey(Key_R);
        AddKey(Key_S);
        AddKey(Key_T);
        AddKey(Key_U);
        AddKey(Key_V);
        AddKey(Key_W);
        AddKey(Key_X);
        AddKey(Key_Y);
        AddKey(Key_Z);
        
        // Number keys
        AddKey(Key_0);
        AddKey(Key_1);
        AddKey(Key_2);
        AddKey(Key_3);
        AddKey(Key_4);
        AddKey(Key_5);
        AddKey(Key_6);
        AddKey(Key_7);
        AddKey(Key_8);
        AddKey(Key_9);
        
        // Function keys
        AddKey(Key_F1);
        AddKey(Key_F2);
        AddKey(Key_F3);
        AddKey(Key_F4);
        AddKey(Key_F5);
        AddKey(Key_F6);
        AddKey(Key_F7);
        AddKey(Key_F8);
        AddKey(Key_F9);
        AddKey(Key_F10);
        AddKey(Key_F11);
        AddKey(Key_F12);
        
        // Arrow keys
        AddKey(Key_Up);
        AddKey(Key_Down);
        AddKey(Key_Left);
        AddKey(Key_Right);
        
        // Other common keys
        AddKey(Key_Space);
        AddKey(Key_Enter);
        AddKey(Key_Escape);
        AddKey(Key_Tab);
        AddKey(Key_Backspace);
        AddKey(Key_Shift);
        AddKey(Key_Control);
        AddKey(Key_Alt);
        AddKey(Key_CapsLock);
        AddKey(Key_LeftShift);
        AddKey(Key_RightShift);
        AddKey(Key_LeftControl);
        AddKey(Key_RightControl);
        AddKey(Key_LeftAlt);
        AddKey(Key_RightAlt);
    }

    void InputKeys::AddKey(const InputKey &key)
    {
        m_Keys.push_back(key);
    }
}