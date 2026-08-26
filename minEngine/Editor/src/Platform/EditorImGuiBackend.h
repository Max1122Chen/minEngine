#pragma once

struct GLFWwindow;

namespace minEngine
{
    class RHI;
    class VulkanRHI;

    /** ED-F01: OpenGL vs Vulkan ImGui renderer backend selection. */
    class EditorImGuiBackend
    {
    public:
        enum class RendererApi
        {
            OpenGL,
            Vulkan,
        };

        bool Initialize(RendererApi api, GLFWwindow* window);
        void Shutdown();

#if defined(MINENGINE_HAS_VULKAN)
        bool InitializeVulkanRenderer(VulkanRHI& vulkanRhi);
#endif

        void NewFrame();
        void RenderDrawData(RHI* rhi);

        void NotifyFontAtlasRebuilt();
        void InvalidateViewportTextures();

        RendererApi GetRendererApi() const { return m_Api; }
        bool IsInitialized() const { return m_Initialized; }

    private:
#if defined(MINENGINE_HAS_VULKAN)
        static void OnVulkanSwapchainRecreated();
        bool InitializeVulkan(VulkanRHI& vulkanRhi);
        void ShutdownVulkan();
        void RenderVulkanDrawData(VulkanRHI& vulkanRhi);
#endif

        RendererApi m_Api = RendererApi::OpenGL;
        bool m_Initialized = false;
#if defined(MINENGINE_HAS_VULKAN)
        bool m_VulkanBackendInitialized = false;
        VulkanRHI* m_VulkanRhi = nullptr;
        static EditorImGuiBackend* s_ActiveVulkanBackend;
#endif
    };
}
