#pragma once

struct GLFWwindow;

namespace minEngine
{
    class RHI;
    class VulkanRHI;

    /** OpenGL vs Vulkan ImGui renderer backend selection. */
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

        /**
         * GLFW / input NewFrame — call on the window (Game) thread only.
         * Safe without a current GL/Vulkan context.
         */
        void PlatformNewFrame();

        /**
         * Renderer backend NewFrame (OpenGL3 / Vulkan device objects).
         * Call on the thread that owns the graphics context / queue.
         */
        void RendererNewFrame();

        /** PlatformNewFrame then RendererNewFrame (Single-thread Editor loop). */
        void NewFrame();

        /**
         * ImGui::Render() if draw data is not built yet, then GPU submit.
         * Single-thread convenience; Multi: call BuildDrawData on Game, SubmitDrawData on graphics.
         */
        void RenderDrawData(RHI* rhi);

        /** CPU: finalize the ImGui frame into ImDrawData (window thread). */
        void BuildDrawData();

        /** GPU: upload textures + render ImDrawData (graphics-context thread). */
        void SubmitDrawData(RHI* rhi);

        void NotifyFontAtlasRebuilt();
        void InvalidateViewportTextures();
        void UploadPendingPlatformTextures();

        RendererApi GetRendererApi() const { return m_Api; }
        bool IsInitialized() const { return m_Initialized; }

    private:
#if defined(MINENGINE_HAS_VULKAN)
        static void OnVulkanSwapchainRecreated();
        bool InitializeVulkan(VulkanRHI& vulkanRhi);
        void ShutdownVulkan();
        void RenderVulkanDrawData(VulkanRHI& vulkanRhi);
#endif

        /** Hand the device-touching texture calls of the current API to the texture registry. */
        void InstallBackendOps();

        /**
         * Destroy and then recreate the renderer device objects. Graphics role only: it destroys
         * the descriptor pool the registered viewport textures live in.
         */
        void RebuildImGuiDeviceObjects(RHI* rhi);

        RendererApi m_Api = RendererApi::OpenGL;
        bool m_Initialized = false;
#if defined(MINENGINE_HAS_VULKAN)
        bool m_VulkanBackendInitialized = false;
        VulkanRHI* m_VulkanRhi = nullptr;
        static EditorImGuiBackend* s_ActiveVulkanBackend;
#endif
    };
}
