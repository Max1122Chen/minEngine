#include "GLFWWindowSystem.h"
#include "WindowSystem.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"

#include "Core.h"




// TODO: Include error handling and logging as needed



namespace minEngine
{
    WindowSystem* WindowSystem::s_Instance = nullptr;

    void WindowSystem::SetInstance(WindowSystem* instance)
    {
        s_Instance = instance;
    }

    WindowSystem& WindowSystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "WindowSystem is not initialized");
        return *s_Instance;
    }

    GLFWWindowSystem::~GLFWWindowSystem()
    {
        Shutdown();
    }

    void GLFWWindowSystem::Initialize()
    {
        if (m_IsInitialized)
        {
            return;
        }

        if (!glfwInit())
        {
            // Initialization failed
            ME_CORE_ERROR("Failed to initialize GLFW");
            return;
        }
        m_IsGlfwInitialized = true;

        const bool useVulkan = RHIBackendSelection::IsVulkan();
        if (useVulkan)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        else
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }

        // Create the window
        m_Window = glfwCreateWindow(m_Width, m_Height, "minEngine Window", nullptr, nullptr);
        if (!m_Window)
        {
            // Window creation failed
            ME_CORE_ERROR("Failed to create GLFW window");
            glfwTerminate();
            m_IsGlfwInitialized = false;
            return;
        }

        if (useVulkan)
        {
            m_HasOpenGLContext = false;
            glfwSetWindowUserPointer(m_Window, this);
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            SetupWindowEventCallbacks();
            ME_CORE_INFO("GLFW Window Initialized (Vulkan / NO_API)");
            m_IsInitialized = true;
            return;
        }

        m_HasOpenGLContext = true;
        glfwMakeContextCurrent(m_Window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            // GLAD initialization failed
            ME_CORE_ERROR("Failed to initialize GLAD");
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            glfwTerminate();
            m_IsGlfwInitialized = false;
            return;
        }

        // Set the viewport
        glViewport(0, 0, m_Width, m_Height);

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        SetupWindowEventCallbacks();

        RegisterOnWindowSizeCallback([this](int width, int height)
        {
            glViewport(0, 0, width, height);
        });

        ME_CORE_INFO("GLFW Window Initialized (OpenGL 4.6)");
        m_IsInitialized = true;
    }

    // Shutdown and clean up resources
    void GLFWWindowSystem::Shutdown()
    {
        if (!m_IsInitialized && !m_IsGlfwInitialized && m_Window == nullptr)
        {
            return;
        }

        if (m_Window)
        {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        
        if (m_IsGlfwInitialized)
        {
            glfwTerminate();
            m_IsGlfwInitialized = false;
            ME_CORE_INFO("GLFW Terminated");
        }

        m_IsInitialized = false;
    }

    // Check if the window should close
    bool GLFWWindowSystem::ShouldClose() const { return glfwWindowShouldClose(m_Window); }

    void GLFWWindowSystem::Close()
    {
        glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
    }

    void GLFWWindowSystem::SetTitle(const char *title) { glfwSetWindowTitle(m_Window, title);}

    void GLFWWindowSystem::SetClearColor(Vector3 color)
    {
        if (!m_HasOpenGLContext)
        {
            return;
        }
        glClearColor(color.x, color.y, color.z, 1.0f);
    }

    // Clear the window
    void GLFWWindowSystem::Clear()
    {
        if (!m_HasOpenGLContext)
        {
            return;
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    // Swap the front and back buffers
    void GLFWWindowSystem::SwapBuffers()
    {
        if (!m_HasOpenGLContext || m_Window == nullptr)
        {
            return;
        }
        glfwSwapBuffers(m_Window);
    }

    // Poll events
    void GLFWWindowSystem::PollEvents() const { glfwPollEvents(); }

    void GLFWWindowSystem::SetCursorVisible(bool visible)
    {
        glfwSetInputMode(m_Window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    void GLFWWindowSystem::OnKey(InputKey key, int scancode, InputKeyAction action, int mods)
    {
        for (const auto& callback : m_OnKeyCallbacks)
        {
            callback(key, scancode, action, mods);
        }
    }

    void GLFWWindowSystem::OnMouseButton(InputKey key, InputKeyAction action, int mods)
    {
        for (const auto& callback : m_OnMouseButtonCallbacks)
        {
            callback(key, action, mods);
        }
    }

    void GLFWWindowSystem::OnCursorPos(double xPos, double yPos)
    {
        for (const auto& callback : m_OnCursorPosCallbacks)
        {
            callback(xPos, yPos);
        }
    }

    void GLFWWindowSystem::OnMouseScroll(double xOffset, double yOffset)
    {
        for (const auto& callback : m_OnMouseScrollCallbacks)
        {
            callback(xOffset, yOffset);
        }
    }

    void GLFWWindowSystem::OnWindowSize(int width, int height)
    {
        m_Width = width;
        m_Height = height;

        for (const auto& callback : m_OnWindowSizeCallbacks)
        {
            callback(width, height);
        }
    }

    // Set GLFW callbacks
    void GLFWWindowSystem::SetupWindowEventCallbacks()
    {
        // Bind the static callback functions to GLFW
        glfwSetKeyCallback(m_Window, KeyCallback);
        glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
        glfwSetCursorPosCallback(m_Window, CursorPosCallback);
        glfwSetScrollCallback(m_Window, MouseScrollCallback);
        glfwSetWindowSizeCallback(m_Window, WindowSizeCallback);
    }

    const InputKey &GLFWWindowSystem::ConvertGLFWKeyToInputKey(int glfwKey)
    {
        switch(glfwKey)
        {
            // Alphabet keys
            case GLFW_KEY_A:    return InputKeys::Key_A;
            case GLFW_KEY_B:    return InputKeys::Key_B;
            case GLFW_KEY_C:    return InputKeys::Key_C;
            case GLFW_KEY_D:    return InputKeys::Key_D;
            case GLFW_KEY_E:    return InputKeys::Key_E;
            case GLFW_KEY_F:    return InputKeys::Key_F;
            case GLFW_KEY_G:    return InputKeys::Key_G;
            case GLFW_KEY_H:    return InputKeys::Key_H;
            case GLFW_KEY_I:    return InputKeys::Key_I;
            case GLFW_KEY_J:    return InputKeys::Key_J;
            case GLFW_KEY_K:    return InputKeys::Key_K;
            case GLFW_KEY_L:    return InputKeys::Key_L;
            case GLFW_KEY_M:    return InputKeys::Key_M;
            case GLFW_KEY_N:    return InputKeys::Key_N;
            case GLFW_KEY_O:    return InputKeys::Key_O;
            case GLFW_KEY_P:    return InputKeys::Key_P;
            case GLFW_KEY_Q:    return InputKeys::Key_Q;
            case GLFW_KEY_R:    return InputKeys::Key_R;
            case GLFW_KEY_S:    return InputKeys::Key_S;
            case GLFW_KEY_T:    return InputKeys::Key_T;
            case GLFW_KEY_U:    return InputKeys::Key_U;
            case GLFW_KEY_V:    return InputKeys::Key_V;
            case GLFW_KEY_W:    return InputKeys::Key_W;
            case GLFW_KEY_X:    return InputKeys::Key_X;
            case GLFW_KEY_Y:    return InputKeys::Key_Y;
            case GLFW_KEY_Z:    return InputKeys::Key_Z;
            
            // Number keys
            case GLFW_KEY_0:    return InputKeys::Key_0;
            case GLFW_KEY_1:    return InputKeys::Key_1;
            case GLFW_KEY_2:    return InputKeys::Key_2;
            case GLFW_KEY_3:    return InputKeys::Key_3;
            case GLFW_KEY_4:    return InputKeys::Key_4;
            case GLFW_KEY_5:    return InputKeys::Key_5;
            case GLFW_KEY_6:    return InputKeys::Key_6;
            case GLFW_KEY_7:    return InputKeys::Key_7;
            case GLFW_KEY_8:    return InputKeys::Key_8;
            case GLFW_KEY_9:    return InputKeys::Key_9;
            
            // Function keys
            case GLFW_KEY_F1:    return InputKeys::Key_F1;
            case GLFW_KEY_F2:    return InputKeys::Key_F2;
            case GLFW_KEY_F3:    return InputKeys::Key_F3;
            case GLFW_KEY_F4:    return InputKeys::Key_F4;
            case GLFW_KEY_F5:    return InputKeys::Key_F5;
            case GLFW_KEY_F6:    return InputKeys::Key_F6;
            case GLFW_KEY_F7:    return InputKeys::Key_F7;
            case GLFW_KEY_F8:    return InputKeys::Key_F8;
            case GLFW_KEY_F9:    return InputKeys::Key_F9;
            case GLFW_KEY_F10:   return InputKeys::Key_F10;
            case GLFW_KEY_F11:   return InputKeys::Key_F11;
            case GLFW_KEY_F12:   return InputKeys::Key_F12;
            
            // Arrow keys
            case GLFW_KEY_UP:    return InputKeys::Key_Up;
            case GLFW_KEY_DOWN:  return InputKeys::Key_Down;
            case GLFW_KEY_LEFT:  return InputKeys::Key_Left;
            case GLFW_KEY_RIGHT: return InputKeys::Key_Right;
            
            // Other common keys
            case GLFW_KEY_SPACE:     return InputKeys::Key_Space;
            case GLFW_KEY_ENTER:     return InputKeys::Key_Enter;
            case GLFW_KEY_ESCAPE:    return InputKeys::Key_Escape;
            case GLFW_KEY_TAB:       return InputKeys::Key_Tab;
            case GLFW_KEY_BACKSPACE: return InputKeys::Key_Backspace;
            case GLFW_KEY_CAPS_LOCK: return InputKeys::Key_CapsLock;
            
            // Shift keys
            case GLFW_KEY_LEFT_SHIFT:  return InputKeys::Key_LeftShift;
            case GLFW_KEY_RIGHT_SHIFT: return InputKeys::Key_RightShift;
            
            // Control keys
            case GLFW_KEY_LEFT_CONTROL:  return InputKeys::Key_LeftControl;
            case GLFW_KEY_RIGHT_CONTROL: return InputKeys::Key_RightControl;
            
            // Alt keys
            case GLFW_KEY_LEFT_ALT:  return InputKeys::Key_LeftAlt;
            case GLFW_KEY_RIGHT_ALT: return InputKeys::Key_RightAlt;
            
            // Mouse buttons
            case GLFW_MOUSE_BUTTON_LEFT:   return InputKeys::Mouse_Left;
            case GLFW_MOUSE_BUTTON_MIDDLE: return InputKeys::Mouse_Middle;
            case GLFW_MOUSE_BUTTON_RIGHT:  return InputKeys::Mouse_Right;
            
            default:            return InputKeys::AnyKey;
        }
    }
    
    const InputKeyAction GLFWWindowSystem::ConvertGLFWKeyActionToInputKeyAction(int action)
    {
        switch(action)
        {
            case GLFW_PRESS:    return InputKeyAction::Press;
            case GLFW_REPEAT:   return InputKeyAction::Repeat;
            case GLFW_RELEASE:  return InputKeyAction::Release;
            default:            return InputKeyAction::Idle;
        }
    }
}
