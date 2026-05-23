#include "RenderSystem.h"



#include "imgui.h"

#include "imgui/backends/imgui_impl_glfw.h"

#include "imgui/backends/imgui_impl_opengl3.h"



#include "OpenGL/OpenGLRHI.h"

#include "GLFWWindowSystem.h"



#include "glm/gtc/type_ptr.hpp"



#include "Runtime/Core/Paths/PathRegistry.h"
#include "RenderPipeline/RenderPipeline.h"

#include <filesystem>

namespace minEngine

{

    RenderSystem* RenderSystem::s_Instance = nullptr;



    void RenderSystem::SetInstance(RenderSystem* instance)

    {

        s_Instance = instance;

    }



    bool RenderSystem::HasInstance()
    {
        return s_Instance != nullptr;
    }

    RenderSystem& RenderSystem::Get()

    {

        ME_ASSERT(s_Instance != nullptr, "RenderSystem is not initialized");

        return *s_Instance;

    }



    void RenderSystem::Initialize()

    {   

        m_RHI = std::make_shared<OpenGLRHI>();

        m_RHI->Initialize();



        static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SetClearColor(Vector3(0.1f, 0.1f, 0.1f));



        m_RenderPipeline.Initialize();



        ME_CORE_INFO("RenderSystem Initialized");

    }



    void RenderSystem::LoadEngineRenderingAssets()
    {
        if (m_EngineRenderingAssetsLoaded)
        {
            return;
        }

        const std::filesystem::path& assetsRoot = PathRegistry::Get().GetEngineDefaultAssetsRoot();
        if (assetsRoot.empty())
        {
            ME_CORE_WARN(
                "RenderSystem: EngineDefaultAssetsRoot is empty; skipping IBL / SkyBox load (check EngineConfig).");
            return;
        }

        m_RenderPipeline.LoadEngineRenderingAssets(assetsRoot.string());
        m_EngineRenderingAssetsLoaded = true;
        ME_CORE_INFO("RenderSystem: engine rendering assets loaded.");
    }

    void RenderSystem::ReloadEngineRenderingAssets(const std::string& engineDefaultAssetsRoot)
    {
        if (engineDefaultAssetsRoot.empty())
        {
            return;
        }

        m_RenderPipeline.LoadEngineRenderingAssets(engineDefaultAssetsRoot);
        m_EngineRenderingAssetsLoaded = true;
    }

    void RenderSystem::Shutdown()

    {
        m_EngineRenderingAssetsLoaded = false;

        m_RenderPipeline.Shutdown();



        if (m_RHI)

        {

            m_RHI->Shutdown();

            m_RHI.reset();

        }



        m_PendingDraws.clear();



        ME_CORE_INFO("RenderSystem Shutdown");

    }



    void RenderSystem::SubmitSceneDraw(const SceneDrawDesc& desc)

    {

        m_PendingDraws.push_back(desc);

    }



    void RenderSystem::Tick(float deltaTime)

    {

        (void)deltaTime;



        static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->Clear();



        for (const SceneDrawDesc& drawDesc : m_PendingDraws)

        {

            m_RenderPipeline.Execute(drawDesc);

        }

        m_PendingDraws.clear();

    }



    void RenderSystem::SetPresentPassEnabled(bool enabled)

    {

        m_RenderPipeline.SetPresentPassEnabled(enabled);

    }

}

