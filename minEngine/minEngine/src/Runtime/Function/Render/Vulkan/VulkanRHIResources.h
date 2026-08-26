#pragma once

#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIPipelineLayout.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(MINENGINE_HAS_VULKAN)
#include <vulkan/vulkan.h>
#endif

namespace minEngine
{
#if defined(MINENGINE_HAS_VULKAN)
    /** Shared Vulkan device handles for resource create/upload (RND-F05-S07a). */
    struct VulkanDeviceContext
    {
        VkDevice Device = VK_NULL_HANDLE;
        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        VkQueue GraphicsQueue = VK_NULL_HANDLE;
        VkCommandPool CommandPool = VK_NULL_HANDLE;

        bool IsValid() const
        {
            return Device != VK_NULL_HANDLE && PhysicalDevice != VK_NULL_HANDLE;
        }
    };

    /**
     * Device-local allocation helpers shared by Vulkan buffers/textures.
     * Prefer this over anonymous free functions (cpp-style / hard-constraints).
     */
    class VulkanRHIAllocator
    {
    public:
        static uint32_t FindMemoryType(
            VkPhysicalDevice physicalDevice,
            uint32_t typeFilter,
            VkMemoryPropertyFlags properties);

        static bool CreateBuffer(
            const VulkanDeviceContext& context,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags memoryProperties,
            VkBuffer& outBuffer,
            VkDeviceMemory& outMemory);

        static void DestroyBuffer(VkDevice device, VkBuffer& buffer, VkDeviceMemory& memory);

        static bool CreateImage2D(
            const VulkanDeviceContext& context,
            uint32_t width,
            uint32_t height,
            uint32_t mipLevels,
            VkFormat format,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags memoryProperties,
            VkImage& outImage,
            VkDeviceMemory& outMemory);

        static bool CreateImage2DArray(
            const VulkanDeviceContext& context,
            uint32_t width,
            uint32_t height,
            uint32_t arrayLayers,
            uint32_t mipLevels,
            VkFormat format,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags memoryProperties,
            VkImage& outImage,
            VkDeviceMemory& outMemory);

        static bool CreateImageCube(
            const VulkanDeviceContext& context,
            uint32_t faceSize,
            uint32_t mipLevels,
            VkFormat format,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags memoryProperties,
            VkImage& outImage,
            VkDeviceMemory& outMemory);

        static void DestroyImage(VkDevice device, VkImage& image, VkDeviceMemory& memory);

        static bool CreateImageView2D(
            VkDevice device,
            VkImage image,
            VkFormat format,
            VkImageAspectFlags aspect,
            uint32_t mipLevels,
            VkImageView& outView);

        /** Single mip + single array layer (cube face / array slice RT attachment). */
        static bool CreateImageView2DSubresource(
            VkDevice device,
            VkImage image,
            VkFormat format,
            VkImageAspectFlags aspect,
            uint32_t baseMipLevel,
            uint32_t baseArrayLayer,
            VkImageView& outView);

        static bool CreateImageView2DArray(
            VkDevice device,
            VkImage image,
            VkFormat format,
            VkImageAspectFlags aspect,
            uint32_t arrayLayers,
            uint32_t mipLevels,
            VkImageView& outView);

        static bool CreateImageViewCube(
            VkDevice device,
            VkImage image,
            VkFormat format,
            VkImageAspectFlags aspect,
            uint32_t mipLevels,
            VkImageView& outView);

        /** One-shot submit: copy buffer→image and transition to SHADER_READ_ONLY. */
        static bool UploadBufferToImage2D(
            const VulkanDeviceContext& context,
            VkBuffer stagingBuffer,
            VkImage image,
            uint32_t width,
            uint32_t height,
            VkImageAspectFlags aspect);

        /** One-shot submit: copy buffer→cube image (6 array layers) and transition to SHADER_READ_ONLY. */
        static bool UploadBufferToImageCube(
            const VulkanDeviceContext& context,
            VkBuffer stagingBuffer,
            VkImage image,
            uint32_t faceSize,
            VkDeviceSize faceBytes,
            VkImageAspectFlags aspect);

        static bool CopyBuffer(
            const VulkanDeviceContext& context,
            VkBuffer src,
            VkBuffer dst,
            VkDeviceSize size);

        static VkFormat ToVkFormat(TextureFormat format);
        static VkImageAspectFlags AspectFromFormat(TextureFormat format);
        static uint32_t BytesPerPixel(TextureFormat format);
    };
#endif

    /** Vulkan bytecode shader modules (RND-F05). No GLSL string path. */
    class VulkanRHIShader final : public RHIShader
    {
    public:
#if defined(MINENGINE_HAS_VULKAN)
        VulkanRHIShader(VkDevice device, const RHIShaderCreateDesc& desc);
#else
        explicit VulkanRHIShader(const RHIShaderCreateDesc& desc);
#endif
        ~VulkanRHIShader() override;

        VulkanRHIShader(const VulkanRHIShader&) = delete;
        VulkanRHIShader& operator=(const VulkanRHIShader&) = delete;

        bool IsValid() const override { return m_IsValid; }
        const std::string& GetCompileLog() const override { return m_CompileLog; }

#if defined(MINENGINE_HAS_VULKAN)
        VkShaderModule GetVertexModule() const { return m_VertModule; }
        VkShaderModule GetFragmentModule() const { return m_FragModule; }
#endif

    private:
#if defined(MINENGINE_HAS_VULKAN)
        VkDevice m_Device = VK_NULL_HANDLE;
        VkShaderModule m_VertModule = VK_NULL_HANDLE;
        VkShaderModule m_FragModule = VK_NULL_HANDLE;
#endif
        bool m_IsValid = false;
        std::string m_CompileLog;
    };

    class VulkanRHIBuffer final : public RHIBuffer
    {
    public:
#if defined(MINENGINE_HAS_VULKAN)
        VulkanRHIBuffer(
            const VulkanDeviceContext& context,
            const RHIBufferCreateDesc& desc,
            const void* initialData);
#endif
        ~VulkanRHIBuffer() override;

        VulkanRHIBuffer(const VulkanRHIBuffer&) = delete;
        VulkanRHIBuffer& operator=(const VulkanRHIBuffer&) = delete;

        const RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }
        void UpdateSubresource(const void* data, uint32_t offset, uint32_t size) override;

        bool IsValid() const { return m_IsValid; }

#if defined(MINENGINE_HAS_VULKAN)
        VkBuffer GetBuffer() const { return m_Buffer; }
#endif

    private:
        RHIBufferCreateDesc m_Desc;
        bool m_IsValid = false;
#if defined(MINENGINE_HAS_VULKAN)
        VulkanDeviceContext m_Context;
        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_Memory = VK_NULL_HANDLE;
        void* m_Mapped = nullptr;
#endif
    };

    class VulkanRHITexture final : public RHITexture
    {
    public:
#if defined(MINENGINE_HAS_VULKAN)
        VulkanRHITexture(
            const VulkanDeviceContext& context,
            const RHITextureCreateDesc& desc,
            const void* initialData);
#endif
        ~VulkanRHITexture() override;

        VulkanRHITexture(const VulkanRHITexture&) = delete;
        VulkanRHITexture& operator=(const VulkanRHITexture&) = delete;

        const RHITextureCreateDesc& GetDesc() const override { return m_Desc; }
        void* GetNativeResource() const override;

        bool IsValid() const { return m_IsValid; }

#if defined(MINENGINE_HAS_VULKAN)
        VkImage GetImage() const { return m_Image; }
        VkImageView GetImageView() const { return m_ImageView; }
        VkFormat GetVkFormat() const { return m_VkFormat; }
        VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
        void SetCurrentLayout(VkImageLayout layout) { m_CurrentLayout = layout; }
#endif

    private:
        RHITextureCreateDesc m_Desc;
        bool m_IsValid = false;
#if defined(MINENGINE_HAS_VULKAN)
        VulkanDeviceContext m_Context;
        VkImage m_Image = VK_NULL_HANDLE;
        VkDeviceMemory m_Memory = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VkFormat m_VkFormat = VK_FORMAT_UNDEFINED;
        VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
#endif
    };

    class VulkanRHIShaderResourceView final : public RHIShaderResourceView
    {
    public:
#if defined(MINENGINE_HAS_VULKAN)
        VulkanRHIShaderResourceView(VkDevice device, const RHITextureSRVDesc& desc);
#else
        explicit VulkanRHIShaderResourceView(const RHITextureSRVDesc& desc);
#endif
        ~VulkanRHIShaderResourceView() override;

        VulkanRHIShaderResourceView(const VulkanRHIShaderResourceView&) = delete;
        VulkanRHIShaderResourceView& operator=(const VulkanRHIShaderResourceView&) = delete;

        const RHITextureSRVDesc& GetCreateDesc() const override { return m_Desc; }
        bool IsValid() const { return m_IsValid; }

#if defined(MINENGINE_HAS_VULKAN)
        VkImageView GetImageView() const { return m_ImageView; }
#endif

    private:
        RHITextureSRVDesc m_Desc;
        bool m_IsValid = false;
#if defined(MINENGINE_HAS_VULKAN)
        VkDevice m_Device = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        bool m_OwnsView = false;
#endif
    };

    class VulkanRHIVertexInputLayout final : public RHIVertexInputLayout
    {
    public:
        VulkanRHIVertexInputLayout() = default;
        explicit VulkanRHIVertexInputLayout(std::initializer_list<RHIVertexElement> elements);

        const std::vector<RHIVertexElement>& GetElements() const override { return m_Elements; }
        uint32_t GetStride() const override { return m_Stride; }

    private:
        std::vector<RHIVertexElement> m_Elements;
        uint32_t m_Stride = 0;
    };

#if defined(MINENGINE_HAS_VULKAN)
    /** Descriptor set layout: Slot → Vk binding (matches MaterialCompiler set=/binding). */
    class VulkanRHIShaderBindingSetLayout final : public RHIShaderBindingSetLayout
    {
    public:
        VulkanRHIShaderBindingSetLayout(VkDevice device, std::vector<RHIShaderBindingSetLayoutEntry> entries);
        ~VulkanRHIShaderBindingSetLayout() override;

        VulkanRHIShaderBindingSetLayout(const VulkanRHIShaderBindingSetLayout&) = delete;
        VulkanRHIShaderBindingSetLayout& operator=(const VulkanRHIShaderBindingSetLayout&) = delete;

        const std::vector<RHIShaderBindingSetLayoutEntry>& GetEntries() const override { return m_Entries; }
        bool IsValid() const { return m_Layout != VK_NULL_HANDLE; }
        VkDescriptorSetLayout GetVkLayout() const { return m_Layout; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
        std::vector<RHIShaderBindingSetLayoutEntry> m_Entries;
    };

    class VulkanRHIPipelineLayout final : public RHIPipelineLayout
    {
    public:
        VulkanRHIPipelineLayout(VkDevice device, std::vector<RHIShaderBindingSetLayout*> setLayouts);
        ~VulkanRHIPipelineLayout() override;

        VulkanRHIPipelineLayout(const VulkanRHIPipelineLayout&) = delete;
        VulkanRHIPipelineLayout& operator=(const VulkanRHIPipelineLayout&) = delete;

        uint32_t GetShaderBindingSetLayoutCount() const override
        {
            return static_cast<uint32_t>(m_SetLayouts.size());
        }
        RHIShaderBindingSetLayout* GetShaderBindingSetLayout(uint32_t setIndex) const override;
        bool IsValid() const { return m_PipelineLayout != VK_NULL_HANDLE; }
        VkPipelineLayout GetVkPipelineLayout() const { return m_PipelineLayout; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        std::vector<RHIShaderBindingSetLayout*> m_SetLayouts;
    };

    class VulkanRHIShaderBindingSet final : public RHIShaderBindingSet
    {
    public:
        struct DummyImageViews
        {
            VkImageView Image2D = VK_NULL_HANDLE;
            VkImageView Image2DArray = VK_NULL_HANDLE;
            VkImageView ImageCube = VK_NULL_HANDLE;
        };

        VulkanRHIShaderBindingSet(
            VkDevice device,
            VkDescriptorPool pool,
            VkSampler defaultSampler,
            VkBuffer dummyUniformBuffer,
            const DummyImageViews& dummyImageViews,
            RHIShaderBindingSetLayout* layout,
            std::vector<RHIShaderBinding> resources);
        ~VulkanRHIShaderBindingSet() override;

        VulkanRHIShaderBindingSet(const VulkanRHIShaderBindingSet&) = delete;
        VulkanRHIShaderBindingSet& operator=(const VulkanRHIShaderBindingSet&) = delete;

        const RHIShaderBindingSetLayout* GetLayout() const override { return m_Layout; }
        const std::vector<RHIShaderBinding>& GetBindings() const override { return m_Resources; }
        bool IsValid() const { return m_DescriptorSet != VK_NULL_HANDLE; }
        VkDescriptorSet GetVkDescriptorSet() const { return m_DescriptorSet; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorPool m_Pool = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
        RHIShaderBindingSetLayout* m_Layout = nullptr;
        std::vector<RHIShaderBinding> m_Resources;
    };

    /**
     * Graphics PSO with lazy VkPipeline creation keyed by active VkRenderPass.
     * Derives from RHIGraphicsPSOStateFallback so SubmitMeshDrawPacket can find PipelineLayout.
     */
    class VulkanRHIGraphicsPipelineState final : public RHIGraphicsPSOStateFallback
    {
    public:
        VulkanRHIGraphicsPipelineState(VkDevice device, const RHIGraphicsPSODesc& desc);
        ~VulkanRHIGraphicsPipelineState() override;

        VulkanRHIGraphicsPipelineState(const VulkanRHIGraphicsPipelineState&) = delete;
        VulkanRHIGraphicsPipelineState& operator=(const VulkanRHIGraphicsPipelineState&) = delete;

        VkPipeline GetOrCreatePipeline(VkRenderPass renderPass);
        VkPipelineLayout GetVkPipelineLayout() const;

        static VkFormat ToVkVertexFormat(VertexElementType type);
        static VkPrimitiveTopology ToVkTopology(RHIPrimitiveType type);
        static VkCompareOp ToVkCompareOp(RHIDepthCompareFunc func);

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        std::unordered_map<VkRenderPass, VkPipeline> m_PipelinesByRenderPass;
    };
#endif
}
