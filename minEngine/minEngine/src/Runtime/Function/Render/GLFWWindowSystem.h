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
        

    protected:
        // Window events callbacks
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnKey(key, scancode, action, mods);
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

        static void WindowSizeCallback(GLFWwindow* window, int width, int height)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnWindowSize(width, height);
            }
        }

        // 
        void OnKey(int key, int scancode, int action, int mods)
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

        void OnWindowSize(int width, int height)
        {
            for (const auto& callback : m_OnWindowSizeCallbacks)
            {
                callback(width, height);
            }
        }



    private:
        GLFWWindowSystem() = delete;
       


    private:
        void SetupWindowEventCallbacks();

    };
}