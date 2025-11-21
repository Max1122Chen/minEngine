#include "GLFWWindowSystem.h"

#include "Core.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"



// TODO: Include error handling and logging as needed



namespace minEngine
{
    GLFWWindowSystem::~GLFWWindowSystem()
    {
        Shutdown();
    }

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
    bool GLFWWindowSystem::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }

    // Clear the window
    void GLFWWindowSystem::Clear()
    {
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Swap the front and back buffers
    void GLFWWindowSystem::SwapBuffers()
    {
        glfwSwapBuffers(m_Window);
    }

    // Poll for and process events
    void GLFWWindowSystem::PollEvents() const
    {
        glfwPollEvents();
    }

    // Process input (placeholder for actual input handling)
    void GLFWWindowSystem::ProcessInput() const
    {
        if(glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(m_Window, true);
        }
    }

    // Set GLFW callbacks
    void GLFWWindowSystem::SetCallbacks()
    {
        // Set GLFW callbacks here as needed
        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, int width, int height)
            {
                glViewport(0, 0, width, height);
            });
    }


}
