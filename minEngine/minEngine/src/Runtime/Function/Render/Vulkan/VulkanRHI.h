#pragma once

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Vulkan/VulkanRHIResources.h"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#if defined(MINENGINE_HAS_VULKAN)
#include <vulkan/vulkan.h>
#endif

namespace minEngine
{
    /**
     * Vulkan backend (RND-F05): frame recording + descriptor/PSO/cmd path (S07b–d).
     * Frame sync (semaphore/fence) stays private — not exposed on RHI.
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

        struct OffscreenRenderPassKey
        {
            VkFormat ColorFormat = VK_FORMAT_UNDEFINED;
            VkFormat DepthFormat = VK_FORMAT_UNDEFINED;
            VkAttachmentLoadOp ColorLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            VkAttachmentStoreOp ColorStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            VkAttachmentLoadOp DepthLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            VkAttachmentStoreOp DepthStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            bool HasColor = false;
            bool HasDepth = false;

            bool operator==(const OffscreenRenderPassKey& other) const
            {
                return ColorFormat == other.ColorFormat && DepthFormat == other.DepthFormat &&
                    ColorLoadOp == other.ColorLoadOp && ColorStoreOp == other.ColorStoreOp &&
                    DepthLoadOp == other.DepthLoadOp && DepthStoreOp == other.DepthStoreOp &&
                    HasColor == other.HasColor && HasDepth == other.HasDepth;
            }
        };

        struct OffscreenRenderPassKeyHash
        {
            size_t operator()(const OffscreenRenderPassKey& key) const
            {
                size_t h = static_cast<size_t>(key.ColorFormat);
                h ^= static_cast<size_t>(key.DepthFormat) << 1;
                h ^= static_cast<size_t>(key.ColorLoadOp) << 2;
                h ^= static_cast<size_t>(key.DepthLoadOp) << 3;
                h ^= static_cast<size_t>(key.HasColor) << 4;
                h ^= static_cast<size_t>(key.HasDepth) << 5;
                return h;
            }
        };

        struct OffscreenFramebufferKey
        {
            VkRenderPass RenderPass = VK_NULL_HANDLE;
            VkImageView ColorView = VK_NULL_HANDLE;
            VkImageView DepthView = VK_NULL_HANDLE;
            uint32_t Width = 0;
            uint32_t Height = 0;

            bool operator==(const OffscreenFramebufferKey& other) const
            {
                return RenderPass == other.RenderPass && ColorView == other.ColorView &&
                    DepthView == other.DepthView && Width == other.Width && Height == other.Height;
            }
        };

        struct OffscreenFramebufferKeyHash
        {
            size_t operator()(const OffscreenFramebufferKey& key) const
            {
                size_t h = reinterpret_cast<size_t>(key.RenderPass);
                h ^= reinterpret_cast<size_t>(key.ColorView) << 1;
                h ^= reinterpret_cast<size_t>(key.DepthView) << 2;
                h ^= static_cast<size_t>(key.Width) << 3;
                h ^= static_cast<size_t>(key.Height) << 4;
                return h;
            }
        };

        bool CreateInstance();
        bool CreateSurface();
        bool PickPhysicalDevice();
        bool CreateDevice();
        bool CreateSwapchain();
        bool CreateCommandResources();
        bool CreateSyncObjects();
        bool CreateSwapchainRenderPass();
        bool CreateDescriptorResources();
        void DestroyDescriptorResources();
        void DestroySwapchainRenderPass();
        void DestroySwapchain();
        void RecreateSwapchain();
        bool BeginFrameRecording();
        void RecordSwapchainClearPass();
        void EndFrameRecordingAndSubmit();
        VulkanDeviceContext GetDeviceContext() const;

        VkCommandBuffer GetCurrentCommandBuffer() const;
        bool EnsureFrameRecording() const;
        void TransitionImage(
            VkCommandBuffer cmd,
            VkImage image,
            VkImageAspectFlags aspect,
            VkImageLayout oldLayout,
            VkImageLayout newLayout);
        void TransitionTextureTo(
            VulkanRHITexture* texture,
            VkImageLayout newLayout);
        VkAttachmentLoadOp ToVkLoadOp(RHIRenderTargetLoadAction action) const;
        VkAttachmentStoreOp ToVkStoreOp(RHIRenderTargetStoreAction action) const;
        VkRenderPass GetOrCreateOffscreenRenderPass(const OffscreenRenderPassKey& key);
        VkFramebuffer GetOrCreateOffscreenFramebuffer(const OffscreenFramebufferKey& key);
        void BindPipelineForCurrentRenderPass();

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
        std::vector<VkImageLayout> m_SwapchainImageLayouts;

        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        std::array<VkCommandBuffer, kMaxFramesInFlight> m_CommandBuffers{};
        std::array<VkSemaphore, kMaxFramesInFlight> m_ImageAvailableSemaphores{};
        std::array<VkSemaphore, kMaxFramesInFlight> m_RenderFinishedSemaphores{};
        std::array<VkFence, kMaxFramesInFlight> m_InFlightFences{};

        uint32_t m_CurrentFrame = 0;
        uint32_t m_CurrentImageIndex = 0;
        bool m_FrameRecording = false;
        bool m_SwapchainDrawnThisFrame = false;
        bool m_Initialized = false;

        VkRenderPass m_SwapchainRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_SwapchainFramebuffers;

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        VkSampler m_DefaultSampler = VK_NULL_HANDLE;
        VkBuffer m_DummyUniformBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_DummyUniformMemory = VK_NULL_HANDLE;
        VkImage m_DummyImage = VK_NULL_HANDLE;
        VkDeviceMemory m_DummyImageMemory = VK_NULL_HANDLE;
        VkImageView m_DummyImageView = VK_NULL_HANDLE;

        std::unordered_map<OffscreenRenderPassKey, VkRenderPass, OffscreenRenderPassKeyHash> m_OffscreenRenderPasses;
        std::unordered_map<OffscreenFramebufferKey, VkFramebuffer, OffscreenFramebufferKeyHash> m_OffscreenFramebuffers;

        bool m_InRenderPass = false;
        VkRenderPass m_ActiveRenderPass = VK_NULL_HANDLE;
        VulkanRHITexture* m_ActiveColorTexture = nullptr;
        VulkanRHITexture* m_ActiveDepthTexture = nullptr;
        VulkanRHIGraphicsPipelineState* m_BoundGraphicsPSO = nullptr;
        uint32_t m_BoundVertexStride = 0;
        bool m_GenerateMipsWarned = false;
        bool m_BeginFrameFailureLogged = false;
        bool m_PipelineBindFailureLogged = false;
        bool m_DrawIndexedLogged = false;
        uint32_t m_FrameDrawIndexedCount = 0;
        uint32_t m_FrameDrawIndexedCalls = 0;
        uint32_t m_LoggedDrawIndexedFrameCount = 0;
#endif

        Vector3 m_ClearColor{0.1f, 0.1f, 0.1f};
    };
}
