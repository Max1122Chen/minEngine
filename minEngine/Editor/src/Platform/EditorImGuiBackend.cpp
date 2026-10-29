#include "Platform/EditorImGuiBackend.h"

#include "Platform/EditorRHIImGuiTexture.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Vulkan/VulkanRHI.h"

#if defined(MINENGINE_HAS_VULKAN)
#include "Runtime/Function/Render/Vulkan/VulkanRHIResources.h"
#include "imgui/backends/imgui_impl_vulkan.h"

// Neither is part of the backend's public header: recreating the device objects after a font
// atlas rebuild is the one thing the editor needs from them, and the header only offers the
// destroy half. Declared here for the same reason the destroy half was.
bool ImGui_ImplVulkan_CreateDeviceObjects();
void ImGui_ImplVulkan_DestroyDeviceObjects();
#endif

#if defined(MINENGINE_HAS_VULKAN)
void ImGui_ImplVulkan_UpdateTexture(ImTextureData* tex);
#endif

void ImGui_ImplOpenGL3_UpdateTexture(ImTextureData* tex);

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
            ME_LOG(LogEditor, Error, "ImGui Vulkan backend: VkResult={}", static_cast<int>(err));
        }

        /**
         * Allocate the descriptor set ImGui draws a texture with. A device call, so the deferred
         * create queue is what calls it - never the thread that asked for the registration.
         */
        std::uint64_t CreateVulkanTextureIdentifier(RHI* rhi, RHITexture* texture)
        {
            auto* vulkanTexture = dynamic_cast<VulkanRHITexture*>(texture);
            auto* vulkanRhi = dynamic_cast<VulkanRHI*>(rhi);
            if (vulkanTexture == nullptr || vulkanRhi == nullptr || !vulkanTexture->IsValid())
            {
                return 0;
            }

            VulkanRHI::VulkanEditorFrameInfo frameInfo{};
            vulkanRhi->FillEditorFrameInfo(frameInfo);
            if (frameInfo.DefaultSampler == VK_NULL_HANDLE ||
                vulkanTexture->GetImageView() == VK_NULL_HANDLE)
            {
                return 0;
            }

            const VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(
                frameInfo.DefaultSampler,
                vulkanTexture->GetImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            return reinterpret_cast<std::uint64_t>(descriptorSet);
        }

        /** Free it. Runs at a submit batch tail, i.e. after every batch that could name it. */
        void ReleaseVulkanTextureIdentifier(RHI* rhi, std::uint64_t identifier)
        {
            (void)rhi;
            ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(identifier));
        }
    }
#endif

    namespace
    {
        /**
         * The GL backend draws with the texture name the RHI already owns, so registering one is a
         * mapping rather than an allocation - and there is nothing to give back on release.
         */
        std::uint64_t CreateOpenGLTextureIdentifier(RHI* rhi, RHITexture* texture)
        {
            (void)rhi;
            return texture != nullptr ? static_cast<std::uint64_t>(GetRHINativeTextureHandle(texture)) : 0;
        }

        void ReleaseOpenGLTextureIdentifier(RHI* rhi, std::uint64_t identifier)
        {
            (void)rhi;
            (void)identifier;
        }
    }

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
            InstallBackendOps();
            return true;
        }

#if defined(MINENGINE_HAS_VULKAN)
        ImGui_ImplGlfw_InitForVulkan(window, true);
        m_Initialized = true;
        return true;
#else
        ME_LOG(LogEditor, Error, "EditorImGuiBackend: Vulkan renderer requested but MINENGINE_HAS_VULKAN is off.");
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
            ME_LOG(LogEditor, Error, "EditorImGuiBackend: swapchain render pass not ready.");
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
            ME_LOG(LogEditor, Error, "EditorImGuiBackend: ImGui_ImplVulkan_Init failed.");
            return false;
        }

        s_ActiveVulkanBackend = this;
        m_VulkanRhi = &vulkanRhi;
        vulkanRhi.SetEditorSwapchainRecreatedCallback(&EditorImGuiBackend::OnVulkanSwapchainRecreated);
        m_VulkanBackendInitialized = true;
        InstallBackendOps();
        ME_LOG(LogEditor, Info, "EditorImGuiBackend: ImGui Vulkan renderer initialized.");
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
        ME_LOG(LogEditor, Info, "EditorImGuiBackend: swapchain recreated — ImGui pipeline refreshed.");
    }
#endif

    void EditorImGuiBackend::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        // The identifiers were allocations of the backend about to be torn down: a deferred release
        // firing after this point would call a device that no longer exists, so they are dropped
        // with the device objects instead of being freed one batch later.
        EditorImGuiTextureRegistry::UninstallBackendOps();

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

    void EditorImGuiBackend::PlatformNewFrame()
    {
        if (!m_Initialized)
        {
            return;
        }

        ImGui_ImplGlfw_NewFrame();
    }

    void EditorImGuiBackend::RendererNewFrame()
    {
        if (!m_Initialized)
        {
            return;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (m_Api == RendererApi::Vulkan && m_VulkanBackendInitialized)
        {
            ImGui_ImplVulkan_NewFrame();
            return;
        }
#endif
        if (m_Api == RendererApi::OpenGL)
        {
            ImGui_ImplOpenGL3_NewFrame();
        }
    }

    void EditorImGuiBackend::NewFrame()
    {
        // Single-thread order matches historical ImGui samples: renderer then platform.
        RendererNewFrame();
        PlatformNewFrame();
    }

    void EditorImGuiBackend::BuildDrawData()
    {
        if (!m_Initialized)
        {
            return;
        }

        ImGui::Render();
    }

    void EditorImGuiBackend::SubmitDrawData(RHI* rhi)
    {
        if (!m_Initialized)
        {
            return;
        }

        // The GPU half of what the UI thread asked for while it owned nothing the device needs: a
        // font atlas rebuild destroys and recreates the device objects, and the new atlas texture is
        // uploaded right behind it. This is the only place that may happen, so it happens here even
        // on a frame that ends up drawing nothing.
        EditorImGuiTextureRegistry::PerformPendingGpuWork(rhi);

        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData == nullptr)
        {
            return;
        }

        UploadPendingPlatformTextures();

#if defined(MINENGINE_HAS_VULKAN)
        if (m_Api == RendererApi::Vulkan)
        {
            auto* vulkanRhi = dynamic_cast<VulkanRHI*>(rhi);
            if (vulkanRhi == nullptr)
            {
                ME_LOG(LogEditor, Error, "EditorImGuiBackend: Vulkan API selected but RHI is not VulkanRHI.");
                return;
            }

            if (!m_VulkanBackendInitialized)
            {
                ME_LOG(LogEditor, Warn, "EditorImGuiBackend: Vulkan renderer not initialized; skipping ImGui draw.");
                return;
            }

            RenderVulkanDrawData(*vulkanRhi);
            return;
        }
#endif

        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }

    void EditorImGuiBackend::RenderDrawData(RHI* rhi)
    {
        BuildDrawData();
        SubmitDrawData(rhi);
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
            ME_LOG(LogEditor, Warn, "EditorImGuiBackend: BeginEditorSwapchainRenderPass failed (ImGui frame skipped).");
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

        // Only a request: the UI thread has just rebuilt the CPU atlas, and the device objects that
        // keep the old one uploaded are owned by the graphics role. SubmitDrawData performs it,
        // right before the new atlas texture is uploaded.
        EditorImGuiTextureRegistry::RequestFontAtlasRebuild();
    }

    void EditorImGuiBackend::InvalidateViewportTextures()
    {
        // Retirement, not freeing: each identifier is destroyed at the tail of a later batch.
        EditorRHIImGuiTexturePin::InvalidateAllBindings();
    }

    void EditorImGuiBackend::InstallBackendOps()
    {
        EditorImGuiTextureBackendOps ops;
        ops.RebuildDeviceObjects = [this](RHI* rhi) { RebuildImGuiDeviceObjects(rhi); };

        if (m_Api == RendererApi::OpenGL)
        {
            ops.CreateIdentifier = [](RHI* rhi, RHITexture* texture) {
                return CreateOpenGLTextureIdentifier(rhi, texture);
            };
            ops.ReleaseIdentifier = [](RHI* rhi, std::uint64_t identifier) {
                ReleaseOpenGLTextureIdentifier(rhi, identifier);
            };
        }
#if defined(MINENGINE_HAS_VULKAN)
        else
        {
            ops.CreateIdentifier = [](RHI* rhi, RHITexture* texture) {
                return CreateVulkanTextureIdentifier(rhi, texture);
            };
            ops.ReleaseIdentifier = [](RHI* rhi, std::uint64_t identifier) {
                ReleaseVulkanTextureIdentifier(rhi, identifier);
            };
        }
#endif

        EditorImGuiTextureRegistry::InstallBackendOps(std::move(ops));
    }

    void EditorImGuiBackend::RebuildImGuiDeviceObjects(RHI* rhi)
    {
        (void)rhi;
        if (m_Api == RendererApi::OpenGL)
        {
            ImGui_ImplOpenGL3_DestroyDeviceObjects();
            ImGui_ImplOpenGL3_CreateDeviceObjects();
            return;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (m_Api != RendererApi::Vulkan || !m_VulkanBackendInitialized || m_VulkanRhi == nullptr)
        {
            return;
        }

        ImGui_ImplVulkan_DestroyDeviceObjects();
        ImGui_ImplVulkan_CreateDeviceObjects();

        // The pipeline built above came from the render pass recorded at init time, and a swapchain
        // recreation replaces that pass: rebuild it against the pass the device draws into now.
        VulkanRHI::VulkanEditorFrameInfo frameInfo{};
        m_VulkanRhi->FillEditorFrameInfo(frameInfo);
        if (frameInfo.SwapchainRenderPass != VK_NULL_HANDLE)
        {
            ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
            pipelineInfo.RenderPass = frameInfo.SwapchainRenderPass;
            pipelineInfo.Subpass = 0;
            pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
        }
        ME_LOG(LogEditor, Info, "EditorImGuiBackend: ImGui device objects recreated after a font atlas rebuild.");
#endif
    }

    void EditorImGuiBackend::UploadPendingPlatformTextures()
    {
        if (!m_Initialized)
        {
            return;
        }

        ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
        for (ImTextureData* textureData : platformIo.Textures)
        {
            if (textureData == nullptr || textureData->Status == ImTextureStatus_OK)
            {
                continue;
            }

            if (m_Api == RendererApi::OpenGL)
            {
                ImGui_ImplOpenGL3_UpdateTexture(textureData);
                continue;
            }

#if defined(MINENGINE_HAS_VULKAN)
            if (m_Api == RendererApi::Vulkan && m_VulkanBackendInitialized)
            {
                ImGui_ImplVulkan_UpdateTexture(textureData);
            }
#endif
        }
    }
}
