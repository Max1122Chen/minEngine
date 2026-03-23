#include "RenderSystem.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "OpenGL/OpenGLRHI.h"
#include "GLFWWindowSystem.h"
#include "RenderCamera.h"




#include "RuntimeGlobalContext.h"

#include "RenderScene.h"

#include "glm/gtc/type_ptr.hpp"

#include "RenderPasses/MainCameraPass.h"

namespace minEngine
{
    void RenderSystem::Initialize()
    {   
        // TODO : Create RHI based on configuration
        m_RHI = std::make_shared<OpenGLRHI>();
        m_RHI->Initialize();

        // Create RenderCamera
        m_MainCamera = std::make_shared<RenderCamera>();
        m_MainCamera->Initialize();

        // Create RenderScene
        m_RenderScene = std::make_shared<RenderScene>();

        // set clear color
        static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SetClearColor(Vector3(0.1f, 0.1f, 0.1f));


        // Finished Initialization
        ME_CORE_INFO("RenderSystem Initialized");
    }

    void RenderSystem::Shutdown()
    {
        // TODO: implement shutdown logic
        ME_CORE_INFO("RenderSystem Shutdown");
    }

    RenderSystem &RenderSystem::GetRenderSystem()
    {
        return *RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem;
    }

    void RenderSystem::Tick(float deltaTime)
    {
        // Clear the window
        static_cast<OpenGLRHI*>(m_RHI.get())-> m_WindowSystem->Clear();

        MainCameraPass mainCameraPass;
        mainCameraPass.Render();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SwapBuffers();
    }

}