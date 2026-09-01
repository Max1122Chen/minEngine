#include "Platform/EditorImGuiBackend.h"

#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Vulkan/VulkanRHI.h"

#if defined(MINENGINE_HAS_VULKAN)
#include "imgui/backends/imgui_impl_vulkan.h"
#endif

namespace minEngine
{
#if defined(MINENGINE_HAS_VULKAN)
    EditorImGuiBackend* EditorImGuiBackend::s_ActiveVulkanBackend = nullptr;

    namespace
    {
        void CheckVkResultForImGui(VkResult err)
        {
            if (err == VK_SUCCESS)
            {
                return;
            }
            ME_CORE_ERROR("ImGui Vulkan backend: VkResult={}", static_cast<int>(err));
        }
    }
#endif

    bool EditorImGuiBackend::Initialize(RendererApi api, GLFWwindow* window)
    {
        if (m_Initialized || window == nullptr)
        {
            return false;
        }

        m_Api = api;

        if (api == RendererApi::OpenGL)
        {
            ImGui_ImplGlfw_InitForOpenGL(window, true);
            ImGui_ImplOpenGL3_Init();
            m_Initialized = true;
            return true;
        }

#if defined(MINENGINE_HAS_VULKAN)
        ImGui_ImplGlfw_InitForVulkan(window, true);
        m_Initialized = true;
        return true;
#else
        ME_CORE_ERROR("EditorImGuiBackend: Vulkan renderer requested but MINENGINE_HAS_VULKAN is off.");
        return false;
#endif
    }

#if defined(MINENGINE_HAS_VULKAN)
    bool EditorImGuiBackend::InitializeVulkanRenderer(VulkanRHI& vulkanRhi)
    {
        if (!m_Initialized || m_Api != RendererApi::Vulkan)
        {
            return false;
        }

        return InitializeVulkan(vulkanRhi);
    }

    bool EditorImGuiBackend::InitializeVulkan(VulkanRHI& vulkanRhi)
    {
        if (m_VulkanBackendInitialized)
        {
            return true;
        }

        VulkanRHI::VulkanEditorFrameInfo frameInfo{};
        vulkanRhi.FillEditorFrameInfo(frameInfo);
        if (frameInfo.SwapchainRenderPass == VK_NULL_HANDLE)
        {
            ME_CORE_ERROR("EditorImGuiBackend: swapchain render pass not ready.");
            return false;
        }

        ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
        pipelineInfo.RenderPass = frameInfo.SwapchainRenderPass;
        pipelineInfo.Subpass = 0;
        pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_2;
        initInfo.Instance = frameInfo.Instance;
        initInfo.PhysicalDevice = frameInfo.PhysicalDevice;
        initInfo.Device = frameInfo.Device;
        initInfo.QueueFamily = frameInfo.QueueFamily;
        initInfo.Queue = frameInfo.Queue;
        initInfo.DescriptorPoolSize = 256;
        initInfo.MinImageCount = frameInfo.MinSwapchainImageCount > 0 ? frameInfo.MinSwapchainImageCount : 2;
        initInfo.ImageCount =
            frameInfo.SwapchainImageCount > 0 ? frameInfo.SwapchainImageCount : initInfo.MinImageCount;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.PipelineInfoMain = pipelineInfo;
        initInfo.PipelineInfoForViewports = pipelineInfo;
        initInfo.UseDynamicRendering = false;
        initInfo.CheckVkResultFn = CheckVkResultForImGui;
        initInfo.MinAllocationSize = 1024 * 1024;

        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            ME_CORE_ERROR("EditorImGuiBackend: ImGui_ImplVulkan_Init failed.");
            return false;
        }

        s_ActiveVulkanBackend = this;
        m_VulkanRhi = &vulkanRhi;
        vulkanRhi.SetEditorSwapchainRecreatedCallback(&EditorImGuiBackend::OnVulkanSwapchainRecreated);
        m_VulkanBackendInitialized = true;
        ME_CORE_INFO("EditorImGuiBackend: ImGui Vulkan renderer initialized.");
        return true;
    }

    void EditorImGuiBackend::OnVulkanSwapchainRecreated()
    {
        if (s_ActiveVulkanBackend == nullptr || s_ActiveVulkanBackend->m_VulkanRhi == nullptr)
        {
            return;
        }

        VulkanRHI::VulkanEditorFrameInfo frameInfo{};
        s_ActiveVulkanBackend->m_VulkanRhi->FillEditorFrameInfo(frameInfo);

        ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
        pipelineInfo.RenderPass = frameInfo.SwapchainRenderPass;
        pipelineInfo.Subpass = 0;
        pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);

        const uint32_t minImageCount =
            frameInfo.MinSwapchainImageCount > 0 ? frameInfo.MinSwapchainImageCount : 2;
        ImGui_ImplVulkan_SetMinImageCount(minImageCount);
        s_ActiveVulkanBackend->InvalidateViewportTextures();
        ME_CORE_INFO("EditorImGuiBackend: swapchain recreated — ImGui pipeline refreshed.");
    }
#endif

    void EditorImGuiBackend::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (m_VulkanBackendInitialized)
        {
            ShutdownVulkan();
        }
        else if (m_Api == RendererApi::OpenGL)
#endif
        {
            ImGui_ImplOpenGL3_Shutdown();
        }

        ImGui_ImplGlfw_Shutdown();
        m_Initialized = false;
    }

#if defined(MINENGINE_HAS_VULKAN)
    void EditorImGuiBackend::ShutdownVulkan()
    {
        if (!m_VulkanBackendInitialized)
        {
            return;
        }

        if (s_ActiveVulkanBackend == this)
        {
            if (m_VulkanRhi != nullptr)
            {
                m_VulkanRhi->SetEditorSwapchainRecreatedCallback(nullptr);
            }
            s_ActiveVulkanBackend = nullptr;
        }

        m_VulkanRhi = nullptr;

        ImGui_ImplVulkan_Shutdown();
        m_VulkanBackendInitialized = false;
    }
#endif

    void EditorImGuiBackend::NewFrame()
    {
        if (!m_Initialized)
        {
            return;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (m_Api == RendererApi::Vulkan && m_VulkanBackendInitialized)
        {
            ImGui_ImplVulkan_NewFrame();
        }
        else if (m_Api == RendererApi::OpenGL)
#endif
        {
            ImGui_ImplOpenGL3_NewFrame();
        }

        ImGui_ImplGlfw_NewFrame();
    }

    void EditorImGuiBackend::RenderDrawData(RHI* rhi)
    {
        if (!m_Initialized)
        {
            return;
        }

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData == nullptr)
        {
            return;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (m_Api == RendererApi::Vulkan)
        {
            auto* vulkanRhi = dynamic_cast<VulkanRHI*>(rhi);
            if (vulkanRhi == nullptr)
            {
                ME_CORE_ERROR("EditorImGuiBackend: Vulkan API selected but RHI is not VulkanRHI.");
                return;
            }

            if (!m_VulkanBackendInitialized)
            {
                ME_CORE_WARN("EditorImGuiBackend: Vulkan renderer not initialized; skipping ImGui draw.");
                return;
            }

            RenderVulkanDrawData(*vulkanRhi);
            return;
        }
#endif

        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }

#if defined(MINENGINE_HAS_VULKAN)
    void EditorImGuiBackend::RenderVulkanDrawData(VulkanRHI& vulkanRhi)
    {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData == nullptr || !vulkanRhi.IsEditorFrameRecording())
        {
            return;
        }

        if (!vulkanRhi.BeginEditorSwapchainRenderPass())
        {
            ME_CORE_WARN("EditorImGuiBackend: BeginEditorSwapchainRenderPass failed (ImGui frame skipped).");
            return;
        }

        ImGui_ImplVulkan_RenderDrawData(drawData, vulkanRhi.GetEditorCommandBuffer());
        vulkanRhi.EndEditorSwapchainRenderPass();
    }
#endif

    void EditorImGuiBackend::NotifyFontAtlasRebuilt()
    {
        if (!m_Initialized)
        {
            return;
        }

        if (m_Api == RendererApi::OpenGL)
        {
            // ImGui 1.92 dynamic atlas: drop stale GPU textures/shaders so the next NewFrame
            // recreates device objects against the rebuilt atlas.
            ImGui_ImplOpenGL3_DestroyDeviceObjects();
            return;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (m_Api == RendererApi::Vulkan && m_VulkanBackendInitialized)
        {
            InvalidateViewportTextures();
        }
#endif
    }

    void EditorImGuiBackend::InvalidateViewportTextures()
    {
        (void)0;
    }
}
