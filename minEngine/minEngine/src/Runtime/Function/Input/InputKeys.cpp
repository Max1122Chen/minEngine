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


    void InputKeys::Initialize()
    {
        AddKey(AnyKey);

        AddKey(MouseX);
        AddKey(MouseY);
        AddKey(Mouse2D);
        AddKey(MouseScroll);

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
    }

    void InputKeys::AddKey(const InputKey &key)
    {
        m_Keys.push_back(key);
    }

    const InputKey &InputKeys::ConvertGLFWKeyToInputKey(int glfwKey)
    {
        switch(glfwKey)
        {
            case GLFW_KEY_A:
                return Key_A;
            case GLFW_KEY_B:
                return Key_B;
            case GLFW_KEY_C:
                return Key_C;
            case GLFW_KEY_D:
                return Key_D;
            case GLFW_KEY_E:
                return Key_E;
            case GLFW_KEY_F:
                return Key_F;
            case GLFW_KEY_G:
                return Key_G;
            case GLFW_KEY_H:
                return Key_H;
            case GLFW_KEY_I:
                return Key_I;
            case GLFW_KEY_J:
                return Key_J;
            case GLFW_KEY_K:
                return Key_K;
            case GLFW_KEY_L:
                return Key_L;
            case GLFW_KEY_M:
                return Key_M;
            case GLFW_KEY_N:
                return Key_N;
            case GLFW_KEY_O:
                return Key_O;
            case GLFW_KEY_P:
                return Key_P;
            case GLFW_KEY_Q:
                return Key_Q;
            case GLFW_KEY_R:
                return Key_R;
            case GLFW_KEY_S:
                return Key_S;
            case GLFW_KEY_T:
                return Key_T;
            case GLFW_KEY_U:
                return Key_U;
            case GLFW_KEY_V:
                return Key_V;
            case GLFW_KEY_W:
                return Key_W;
            case GLFW_KEY_X:
                return Key_X;
            case GLFW_KEY_Y:
                return Key_Y;
            case GLFW_KEY_Z:
                return Key_Z;
            default:
                return AnyKey;
        }
    }
}