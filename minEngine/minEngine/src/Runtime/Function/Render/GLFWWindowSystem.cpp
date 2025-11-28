#include "GLFWWindowSystem.h"

#include "Core.h"




// TODO: Include error handling and logging as needed



namespace minEngine
{
    GLFWWindowSystem::~GLFWWindowSystem()
    {}

    void GLFWWindowSystem::Initialize()
    {
        if (!glfwInit())
        {
            // Initialization failed
            ME_CORE_ERROR("Failed to initialize GLFW");
            return;
        }

        // Set GLFW window hints here as needed
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Create the window
        m_Window = glfwCreateWindow(m_Width, m_Height, "minEngine Window", nullptr, nullptr);
        if (!m_Window)
        {
            // Window creation failed
            ME_CORE_ERROR("Failed to create GLFW window");
            glfwTerminate();
            return;
        }


        // TODO: maybe we will add vulkan or other rendering API support later
        // Make the OpenGL context current
        glfwMakeContextCurrent(m_Window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            // GLAD initialization failed
            ME_CORE_ERROR("Failed to initialize GLAD");
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

        ME_CORE_INFO("GLFW Window Initialized");
    }

    // Shutdown and clean up resources
    void GLFWWindowSystem::Shutdown()
    {
        if (m_Window)
        {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        ME_CORE_INFO("GLFW Terminated");
        
        glfwTerminate();
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
        glClearColor(color.x, color.y, color.z, 1.0f);
    }

    // Clear the window
    void GLFWWindowSystem::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Swap the front and back buffers
    void GLFWWindowSystem::SwapBuffers() { glfwSwapBuffers(m_Window);}

    // Poll for and process events
    void GLFWWindowSystem::PollEvents() const { glfwPollEvents(); }

    // Set GLFW callbacks
    void GLFWWindowSystem::SetupWindowEventCallbacks()
    {
        // Bind the static callback functions to GLFW
        glfwSetKeyCallback(m_Window, KeyCallback);
        glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
        glfwSetCursorPosCallback(m_Window, CursorPosCallback);
        glfwSetWindowSizeCallback(m_Window, WindowSizeCallback);
    }


}
