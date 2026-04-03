#pragma once
#include "Core.h"
#include "WindowSystem.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"



struct GLFWwindow;

namespace minEngine
{

    class GLFWWindowSystem : public WindowSystem
    {
    public:


        int m_Width;
        int m_Height;
        GLFWwindow* m_Window = nullptr;


        GLFWWindowSystem(int width = 800, int height = 600)
            : m_Width(width), m_Height(height), m_Window(nullptr)
        {};
        
        ~GLFWWindowSystem();

        virtual void Initialize() override;
        virtual void Shutdown() override;
        virtual bool ShouldClose() const override;
        virtual void Close() override;
        virtual void SetTitle(const char* title) override;

        virtual void SetClearColor(Vector3 color) override;
        virtual void Clear() override;

        virtual void SwapBuffers() override;
        virtual void PollEvents() const override;
        virtual void SetCursorVisible(bool visible) override;
        
        virtual void* GetWindowHandle() const override { return m_Window; }

        virtual uint32_t GetWidth() override { return m_Width; }
        virtual uint32_t GetHeight() override { return m_Height; }

    protected:
        // Window events callbacks
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                const InputKey& inputKey = ConvertGLFWKeyToInputKey(key);
                InputKeyAction keyAction = ConvertGLFWKeyActionToInputKeyAction(action);
                
                windowSystem->OnKey(inputKey, scancode, keyAction, mods);
            }
        }

        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnMouseButton(button, action, mods);
            }
        }

        static void CursorPosCallback(GLFWwindow* window, double xPos, double yPos)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnCursorPos(xPos, yPos);
            }
        }

        static void MouseScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnMouseScroll(xOffset, yOffset);
            }
        }

        static void WindowSizeCallback(GLFWwindow* window, int width, int height)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnWindowSize(width, height);
            }
        }

        // 
        void OnKey(InputKey key, int scancode, InputKeyAction action, int mods)
        {
            for (const auto& callback : m_OnKeyCallbacks)
            {
                callback(key, scancode, action, mods);
            }
        }

        void OnMouseButton(int button, int action, int mods)
        {
            for (const auto& callback : m_OnMouseButtonCallbacks)
            {
                callback(button, action);
            }
        }

        void OnCursorPos(double xPos, double yPos)
        {
            for (const auto& callback : m_OnCursorPosCallbacks)
            {
                callback(xPos, yPos);
            }
        }

        void OnMouseScroll(double xOffset, double yOffset)
        {
            for (const auto& callback : m_OnMouseScrollCallbacks)
            {
                callback(xOffset, yOffset);
            }
        }

        void OnWindowSize(int width, int height)
        {
            m_Width = width;
            m_Height = height;

            for (const auto& callback : m_OnWindowSizeCallbacks)
            {
                callback(width, height);
            }
        }



    private:
        GLFWWindowSystem() = delete;

        bool m_IsGlfwInitialized = false;
        bool m_IsInitialized = false;
       


    private:
        void SetupWindowEventCallbacks();
        const static InputKey& ConvertGLFWKeyToInputKey(int glfwKey);
        const static InputKeyAction ConvertGLFWKeyActionToInputKeyAction(int action);

    };
}