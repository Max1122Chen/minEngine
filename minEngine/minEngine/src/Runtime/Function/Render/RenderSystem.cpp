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

#include "RenderPipeline/RenderPipeline.h"

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

        m_RenderPipeline.Initialize();

        // Finished Initialization
        ME_CORE_INFO("RenderSystem Initialized");
    }

    void RenderSystem::Shutdown()
    {
        m_RenderPipeline.Shutdown();

        m_RenderScene.reset();
        m_MainCamera.reset();

        if (m_RHI)
        {
            m_RHI->Shutdown();
            m_RHI.reset();
        }

        ME_CORE_INFO("RenderSystem Shutdown");
    }

    RenderSystem &RenderSystem::Get()
    {
        return *RuntimeGlobalContext::Get().m_RenderSystem;
    }

    void RenderSystem::Tick(float deltaTime)
    {
        if (m_HasPendingSceneResize)
        {
            m_RenderPipeline.ResizeSceneTargets(m_PendingSceneWidth, m_PendingSceneHeight);
            m_HasPendingSceneResize = false;
        }

        // Clear the window
        static_cast<OpenGLRHI*>(m_RHI.get())-> m_WindowSystem->Clear();

        m_RenderPipeline.Execute();
    }

    void RenderSystem::SetPresentPassEnabled(bool enabled)
    {
        m_RenderPipeline.SetPresentPassEnabled(enabled);
    }

    const std::shared_ptr<RHITexture2D>& RenderSystem::GetSceneColorTexture() const
    {
        return m_RenderPipeline.GetSceneColorTexture();
    }

    void RenderSystem::RequestSceneViewportResize(float widthRatio, float heightRatio)
    {
        if (widthRatio == 0 || heightRatio == 0)
        {
            return;
        }

        const float epsilon = 0.0001f;
        if (std::abs(widthRatio - 1.0f) < epsilon && std::abs(heightRatio - 1.0f) < epsilon)
        {
             return;
        }

        m_PendingSceneWidth = static_cast<uint32_t>(m_RenderPipeline.GetSceneBufferWidth() * widthRatio);
        m_PendingSceneHeight = static_cast<uint32_t>(m_RenderPipeline.GetSceneBufferHeight() * heightRatio);
        m_HasPendingSceneResize = true;
    }

    Vector2 RenderSystem::GetSceneBufferSize() const
    {
        return m_RenderPipeline.GetSceneBufferSize();
    }
}