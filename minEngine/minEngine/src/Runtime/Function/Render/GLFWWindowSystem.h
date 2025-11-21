#pragma once
#include "WindowSystem.h"


struct GLFWwindow;

namespace minEngine
{
    

    class GLFWWindowSystem : public WindowSystem
    {
    public:
        GLFWWindowSystem(int width = 800, int height = 600)
            : m_Width(width), m_Height(height), m_Window(nullptr)
        {};
        
        ~GLFWWindowSystem();

        void Initialize();
        void Shutdown();
        bool ShouldClose() const;

        void Clear();

        void SwapBuffers();
        void PollEvents() const;
        void ProcessInput() const;


    private:
        GLFWWindowSystem() = delete;

        int m_Width;
        int m_Height;
        GLFWwindow* m_Window = nullptr;

    private:
        void SetCallbacks();

    };
}