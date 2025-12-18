#pragma once
#include "Core.h"

namespace minEngine
{
    enum class InputAxisType : uint8_t
    {
        None,
        Button,
        Axis1D,
        Axis2D,
        Axis3D
    };
    
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
        static const InputKey MouseScrollUp;
        static const InputKey MouseScrollDown;

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

    public:
        void Initialize();
        void AddKey(const InputKey& key);

        const std::vector<InputKey>& GetAllKeys() const { return m_Keys; }


        // Support function for converting key codes in different APIs to InputKey
        static const InputKey& ConvertGLFWKeyToInputKey(int glfwKey);
    
    private:
        std::vector<InputKey> m_Keys;

    };
}