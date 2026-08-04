#pragma once

#include "Runtime/Function/Render/RHI/RHI.h"

#include <array>
#include <cstdint>
#include <vector>

#if defined(MINENGINE_HAS_VULKAN)
#include <vulkan/vulkan.h>
#endif

namespace minEngine
{
    /**
     * Vulkan backend (RND-F05-S03): instance/device/swapchain Clear+Present.
     * Frame sync (semaphore/fence) stays private — not exposed on RHI.
     * Resource/draw APIs are stubs until later slices.
     */
    class VulkanRHI final : public RHI
    {
    public:
        VulkanRHI() = default;
        ~VulkanRHI() override;

        void Initialize() override;
        void Shutdown() override;

        std::shared_ptr<RHITexture> RHICreateTexture2D(
            const RHITextureCreateDesc& desc,
            const void* initialData) override;
        std::shared_ptr<RHIShaderResourceView> RHICreateShaderResourceView(
            const RHITextureSRVDesc& desc) override;
        std::shared_ptr<RHIBuffer> RHICreateBuffer(
            const RHIBufferCreateDesc& desc,
            const void* initialData) override;
        std::shared_ptr<RHIShader> RHICreateShader(
            const RHIShaderCreateDesc& desc,
            std::string* outCompileLog) override;
        std::shared_ptr<RHIShader> RHICreateShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog) override;
        std::shared_ptr<RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const RHIGraphicsPSODesc& desc) override;
        std::shared_ptr<RHIShaderBindingSetLayout> RHICreateShaderBindingSetLayout(
            const std::vector<RHIShaderBindingSetLayoutEntry>& entries) override;
        std::shared_ptr<RHIPipelineLayout> RHICreatePipelineLayout(
            const std::vector<RHIShaderBindingSetLayout*>& setLayouts) override;
        std::shared_ptr<RHIShaderBindingSet> RHICreateShaderBindingSet(
            RHIShaderBindingSetLayout* layout,
            const std::vector<RHIShaderBinding>& resources) override;
        std::shared_ptr<RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<RHIVertexElement> elements) override;

        void RHICmdBeginRenderPass(const RHIRenderPassInfo& info) override;
        void RHICmdEndRenderPass() override;
        void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState) override;
        void RHICmdSetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet) override;
        void RHICmdTransition(const RHITextureTransitionInfo& transition) override;
        void RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        void RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot) override;
        void RHICmdSetIndexBuffer(RHIBuffer* indexBuffer) override;
        void RHICmdDrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
        void RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex) override;
        void RHICmdGenerateMips(RHITexture* texture) override;

        void RHISetBackbufferClearColor(const Vector3& color) override;
        void RHIClearBackbuffer() override;
        void RHIPresent() override;

    private:
#if defined(MINENGINE_HAS_VULKAN)
        static constexpr uint32_t kMaxFramesInFlight = 2;

        bool CreateInstance();
        bool CreateSurface();
        bool PickPhysicalDevice();
        bool CreateDevice();
        bool CreateSwapchain();
        bool CreateCommandResources();
        bool CreateSyncObjects();
        void DestroySwapchain();
        void RecreateSwapchain();
        bool RecordClearCommands(uint32_t imageIndex);

        VkInstance m_Instance = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        uint32_t m_GraphicsQueueFamily = 0;

        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        VkFormat m_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkExtent2D m_SwapchainExtent{};
        std::vector<VkImage> m_SwapchainImages;
        std::vector<VkImageView> m_SwapchainImageViews;

        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        std::array<VkCommandBuffer, kMaxFramesInFlight> m_CommandBuffers{};
        std::array<VkSemaphore, kMaxFramesInFlight> m_ImageAvailableSemaphores{};
        std::array<VkSemaphore, kMaxFramesInFlight> m_RenderFinishedSemaphores{};
        std::array<VkFence, kMaxFramesInFlight> m_InFlightFences{};

        uint32_t m_CurrentFrame = 0;
        uint32_t m_CurrentImageIndex = 0;
        bool m_FramePrepared = false;
        bool m_Initialized = false;
#endif

        Vector3 m_ClearColor{0.1f, 0.1f, 0.1f};
    };
}
