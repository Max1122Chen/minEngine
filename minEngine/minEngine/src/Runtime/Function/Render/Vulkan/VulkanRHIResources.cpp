#include "VulkanRHIResources.h"
#include "VulkanRHI.h"

#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <glm/detail/type_half.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace minEngine
{
#if defined(MINENGINE_HAS_VULKAN)
    namespace
    {
        // OpenGL uploads TextureFormat::*16F with GL_FLOAT (float32 CPU pixels).
        // Vulkan stores R16G16B16A16_SFLOAT — convert here to match that contract.
        uint16_t Float32ToHalfBits(float value)
        {
            return static_cast<uint16_t>(glm::detail::toFloat16(value));
        }

        void PackFloatRgbToHalfRgba(const float* srcRgb, uint16_t* dstRgba, uint32_t pixelCount)
        {
            for (uint32_t i = 0; i < pixelCount; ++i)
            {
                dstRgba[i * 4 + 0] = Float32ToHalfBits(srcRgb[i * 3 + 0]);
                dstRgba[i * 4 + 1] = Float32ToHalfBits(srcRgb[i * 3 + 1]);
                dstRgba[i * 4 + 2] = Float32ToHalfBits(srcRgb[i * 3 + 2]);
                dstRgba[i * 4 + 3] = Float32ToHalfBits(1.0f);
            }
        }

        void PackFloatRgbaToHalfRgba(const float* srcRgba, uint16_t* dstRgba, uint32_t pixelCount)
        {
            for (uint32_t i = 0; i < pixelCount; ++i)
            {
                dstRgba[i * 4 + 0] = Float32ToHalfBits(srcRgba[i * 4 + 0]);
                dstRgba[i * 4 + 1] = Float32ToHalfBits(srcRgba[i * 4 + 1]);
                dstRgba[i * 4 + 2] = Float32ToHalfBits(srcRgba[i * 4 + 2]);
                dstRgba[i * 4 + 3] = Float32ToHalfBits(srcRgba[i * 4 + 3]);
            }
        }
    }
#endif

#if defined(MINENGINE_HAS_VULKAN)
    uint32_t VulkanRHIAllocator::FindMemoryType(
        VkPhysicalDevice physicalDevice,
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1u << i)) != 0u &&
                (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        return UINT32_MAX;
    }

    bool VulkanRHIAllocator::CreateBuffer(
        const VulkanDeviceContext& context,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryProperties,
        VkBuffer& outBuffer,
        VkDeviceMemory& outMemory)
    {
        outBuffer = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;
        if (!context.IsValid() || size == 0)
        {
            return false;
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(context.Device, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkCreateBuffer failed.");
            return false;
        }

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(context.Device, outBuffer, &requirements);
        const uint32_t memoryTypeIndex =
            FindMemoryType(context.PhysicalDevice, requirements.memoryTypeBits, memoryProperties);
        if (memoryTypeIndex == UINT32_MAX)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: no suitable buffer memory type.");
            vkDestroyBuffer(context.Device, outBuffer, nullptr);
            outBuffer = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;
        if (vkAllocateMemory(context.Device, &allocInfo, nullptr, &outMemory) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkAllocateMemory(buffer) failed.");
            vkDestroyBuffer(context.Device, outBuffer, nullptr);
            outBuffer = VK_NULL_HANDLE;
            return false;
        }

        if (vkBindBufferMemory(context.Device, outBuffer, outMemory, 0) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkBindBufferMemory failed.");
            DestroyBuffer(context.Device, outBuffer, outMemory);
            return false;
        }
        return true;
    }

    void VulkanRHIAllocator::DestroyBuffer(VkDevice device, VkBuffer& buffer, VkDeviceMemory& memory)
    {
        if (device == VK_NULL_HANDLE)
        {
            return;
        }
        if (buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }

    bool VulkanRHIAllocator::CreateImage2D(
        const VulkanDeviceContext& context,
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memoryProperties,
        VkImage& outImage,
        VkDeviceMemory& outMemory)
    {
        outImage = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;
        if (!context.IsValid() || width == 0 || height == 0 || mipLevels == 0)
        {
            return false;
        }

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(context.Device, &imageInfo, nullptr, &outImage) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkCreateImage failed.");
            return false;
        }

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(context.Device, outImage, &requirements);
        const uint32_t memoryTypeIndex =
            FindMemoryType(context.PhysicalDevice, requirements.memoryTypeBits, memoryProperties);
        if (memoryTypeIndex == UINT32_MAX)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: no suitable image memory type.");
            vkDestroyImage(context.Device, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;
        if (vkAllocateMemory(context.Device, &allocInfo, nullptr, &outMemory) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkAllocateMemory(image) failed.");
            vkDestroyImage(context.Device, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            return false;
        }

        if (vkBindImageMemory(context.Device, outImage, outMemory, 0) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkBindImageMemory failed.");
            DestroyImage(context.Device, outImage, outMemory);
            return false;
        }
        return true;
    }

    bool VulkanRHIAllocator::CreateImage2DArray(
        const VulkanDeviceContext& context,
        uint32_t width,
        uint32_t height,
        uint32_t arrayLayers,
        uint32_t mipLevels,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memoryProperties,
        VkImage& outImage,
        VkDeviceMemory& outMemory)
    {
        outImage = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;
        if (!context.IsValid() || width == 0 || height == 0 || arrayLayers == 0 || mipLevels == 0)
        {
            return false;
        }

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(context.Device, &imageInfo, nullptr, &outImage) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkCreateImage(2DArray) failed.");
            return false;
        }

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(context.Device, outImage, &requirements);
        const uint32_t memoryTypeIndex =
            FindMemoryType(context.PhysicalDevice, requirements.memoryTypeBits, memoryProperties);
        if (memoryTypeIndex == UINT32_MAX)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: no suitable 2D array image memory type.");
            vkDestroyImage(context.Device, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;
        if (vkAllocateMemory(context.Device, &allocInfo, nullptr, &outMemory) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkAllocateMemory(2D array image) failed.");
            vkDestroyImage(context.Device, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            return false;
        }

        if (vkBindImageMemory(context.Device, outImage, outMemory, 0) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkBindImageMemory(2DArray) failed.");
            DestroyImage(context.Device, outImage, outMemory);
            return false;
        }
        return true;
    }

    bool VulkanRHIAllocator::CreateImageCube(
        const VulkanDeviceContext& context,
        uint32_t faceSize,
        uint32_t mipLevels,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memoryProperties,
        VkImage& outImage,
        VkDeviceMemory& outMemory)
    {
        outImage = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;
        if (!context.IsValid() || faceSize == 0 || mipLevels == 0)
        {
            return false;
        }

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = faceSize;
        imageInfo.extent.height = faceSize;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 6;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(context.Device, &imageInfo, nullptr, &outImage) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkCreateImage(cube) failed.");
            return false;
        }

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(context.Device, outImage, &requirements);
        const uint32_t memoryTypeIndex =
            FindMemoryType(context.PhysicalDevice, requirements.memoryTypeBits, memoryProperties);
        if (memoryTypeIndex == UINT32_MAX)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: no suitable cube image memory type.");
            vkDestroyImage(context.Device, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;
        if (vkAllocateMemory(context.Device, &allocInfo, nullptr, &outMemory) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkAllocateMemory(cube image) failed.");
            vkDestroyImage(context.Device, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            return false;
        }

        if (vkBindImageMemory(context.Device, outImage, outMemory, 0) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkBindImageMemory(cube) failed.");
            DestroyImage(context.Device, outImage, outMemory);
            return false;
        }
        return true;
    }

    void VulkanRHIAllocator::DestroyImage(VkDevice device, VkImage& image, VkDeviceMemory& memory)
    {
        if (device == VK_NULL_HANDLE)
        {
            return;
        }
        if (image != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, image, nullptr);
            image = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }

    bool VulkanRHIAllocator::CreateImageView2D(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        uint32_t mipLevels,
        VkImageView& outView)
    {
        outView = VK_NULL_HANDLE;
        if (device == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
        {
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &viewInfo, nullptr, &outView) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkCreateImageView failed.");
            outView = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }

    bool VulkanRHIAllocator::CreateImageView2DSubresource(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        uint32_t baseMipLevel,
        uint32_t baseArrayLayer,
        VkImageView& outView)
    {
        outView = VK_NULL_HANDLE;
        if (device == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
        {
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &viewInfo, nullptr, &outView) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkCreateImageView(2D subresource) failed.");
            outView = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }

    bool VulkanRHIAllocator::CreateImageView2DArray(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        uint32_t arrayLayers,
        uint32_t mipLevels,
        VkImageView& outView)
    {
        outView = VK_NULL_HANDLE;
        if (device == VK_NULL_HANDLE || image == VK_NULL_HANDLE || arrayLayers == 0)
        {
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = arrayLayers;
        if (vkCreateImageView(device, &viewInfo, nullptr, &outView) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkCreateImageView(2DArray) failed.");
            outView = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }

    bool VulkanRHIAllocator::CreateImageViewCube(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect,
        uint32_t mipLevels,
        VkImageView& outView)
    {
        outView = VK_NULL_HANDLE;
        if (device == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
        {
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 6;
        if (vkCreateImageView(device, &viewInfo, nullptr, &outView) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: vkCreateImageView(cube) failed.");
            outView = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }

    bool VulkanRHIAllocator::CopyBuffer(
        const VulkanDeviceContext& context,
        VkBuffer src,
        VkBuffer dst,
        VkDeviceSize size)
    {
        if (!context.IsValid() || context.CommandPool == VK_NULL_HANDLE || context.GraphicsQueue == VK_NULL_HANDLE)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: CopyBuffer requires command pool and queue.");
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = context.CommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(context.Device, &allocInfo, &commandBuffer) != VK_SUCCESS)
        {
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(context.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(context.GraphicsQueue);

        vkFreeCommandBuffers(context.Device, context.CommandPool, 1, &commandBuffer);
        return true;
    }

    bool VulkanRHIAllocator::UploadBufferToImage2D(
        const VulkanDeviceContext& context,
        VkBuffer stagingBuffer,
        VkImage image,
        uint32_t width,
        uint32_t height,
        VkImageAspectFlags aspect)
    {
        if (!context.IsValid() || context.CommandPool == VK_NULL_HANDLE || context.GraphicsQueue == VK_NULL_HANDLE)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: UploadBufferToImage2D requires command pool and queue.");
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = context.CommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(context.Device, &allocInfo, &commandBuffer) != VK_SUCCESS)
        {
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = aspect;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(
            commandBuffer,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(context.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(context.GraphicsQueue);

        vkFreeCommandBuffers(context.Device, context.CommandPool, 1, &commandBuffer);
        return true;
    }

    bool VulkanRHIAllocator::UploadBufferToImageCube(
        const VulkanDeviceContext& context,
        VkBuffer stagingBuffer,
        VkImage image,
        uint32_t faceSize,
        VkDeviceSize faceBytes,
        VkImageAspectFlags aspect)
    {
        if (!context.IsValid() || context.CommandPool == VK_NULL_HANDLE || context.GraphicsQueue == VK_NULL_HANDLE)
        {
            ME_CORE_ERROR("VulkanRHIAllocator: UploadBufferToImageCube requires command pool and queue.");
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = context.CommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(context.Device, &allocInfo, &commandBuffer) != VK_SUCCESS)
        {
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 6;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        std::array<VkBufferImageCopy, 6> regions{};
        for (uint32_t face = 0; face < 6; ++face)
        {
            VkBufferImageCopy& region = regions[face];
            region.bufferOffset = faceBytes * face;
            region.imageSubresource.aspectMask = aspect;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;
            region.imageExtent = {faceSize, faceSize, 1};
        }
        vkCmdCopyBufferToImage(
            commandBuffer,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(regions.size()),
            regions.data());

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(context.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(context.GraphicsQueue);

        vkFreeCommandBuffers(context.Device, context.CommandPool, 1, &commandBuffer);
        return true;
    }

    VkFormat VulkanRHIAllocator::ToVkFormat(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::RED:
            return VK_FORMAT_R8_UNORM;
        case TextureFormat::RGB8:
            // Prefer RGBA8 — R8G8B8_UNORM is poorly supported on many GPUs.
            return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGB16F:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::RGBA16F:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::DEPTH16:
            return VK_FORMAT_D16_UNORM;
        case TextureFormat::DEPTH24:
            // X8_D24 is rarely optimal; prefer D32 on modern GPUs.
            return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::DEPTH32:
            return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::DEPTH24STENCIL8:
            // D24S8 is often unsupported; D32S8 is widely available for depth RTs.
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }

    VkImageAspectFlags VulkanRHIAllocator::AspectFromFormat(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::DEPTH16:
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH32:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case TextureFormat::DEPTH24STENCIL8:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    uint32_t VulkanRHIAllocator::BytesPerPixel(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::RED:
            return 1;
        case TextureFormat::RGB8:
            return 3;
        case TextureFormat::RGBA8:
            return 4;
        case TextureFormat::RGB16F:
            return 6;
        case TextureFormat::RGBA16F:
            return 8;
        case TextureFormat::DEPTH16:
            return 2;
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH32:
        case TextureFormat::DEPTH24STENCIL8:
            return 4;
        default:
            return 0;
        }
    }
#endif

#if defined(MINENGINE_HAS_VULKAN)
    VulkanRHIShader::VulkanRHIShader(VkDevice device, const RHIShaderCreateDesc& desc)
        : m_Device(device)
    {
        if (m_Device == VK_NULL_HANDLE)
        {
            m_CompileLog = "VulkanRHIShader: device is null.";
            return;
        }

        const std::vector<uint32_t>* vertexSpirv = nullptr;
        const std::vector<uint32_t>* fragmentSpirv = nullptr;
        for (const RHIShaderStageBytecode& stage : desc.Stages)
        {
            if (stage.Stage == RHIGraphicsShaderStage::Vertex)
            {
                vertexSpirv = &stage.SpirvWords;
            }
            else if (stage.Stage == RHIGraphicsShaderStage::Pixel)
            {
                fragmentSpirv = &stage.SpirvWords;
            }
        }

        if (vertexSpirv == nullptr || vertexSpirv->empty() || fragmentSpirv == nullptr || fragmentSpirv->empty())
        {
            m_CompileLog = "VulkanRHIShader: vertex and pixel SPIR-V stages are required.";
            return;
        }

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        moduleInfo.codeSize = vertexSpirv->size() * sizeof(uint32_t);
        moduleInfo.pCode = vertexSpirv->data();
        if (vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &m_VertModule) != VK_SUCCESS)
        {
            m_CompileLog = "VulkanRHIShader: vkCreateShaderModule(vertex) failed.";
            return;
        }

        moduleInfo.codeSize = fragmentSpirv->size() * sizeof(uint32_t);
        moduleInfo.pCode = fragmentSpirv->data();
        if (vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &m_FragModule) != VK_SUCCESS)
        {
            vkDestroyShaderModule(m_Device, m_VertModule, nullptr);
            m_VertModule = VK_NULL_HANDLE;
            m_CompileLog = "VulkanRHIShader: vkCreateShaderModule(fragment) failed.";
            return;
        }

        m_IsValid = true;
        if (!desc.DebugName.empty())
        {
            ME_CORE_INFO("VulkanRHIShader: loaded SPIR-V modules '{}'", desc.DebugName);
        }
    }

    VulkanRHIShader::~VulkanRHIShader()
    {
        if (m_Device == VK_NULL_HANDLE)
        {
            return;
        }

        if (m_FragModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_Device, m_FragModule, nullptr);
            m_FragModule = VK_NULL_HANDLE;
        }
        if (m_VertModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_Device, m_VertModule, nullptr);
            m_VertModule = VK_NULL_HANDLE;
        }
    }
#else
    VulkanRHIShader::VulkanRHIShader(const RHIShaderCreateDesc& desc)
    {
        (void)desc;
        m_CompileLog = "VulkanRHIShader: built without MINENGINE_HAS_VULKAN.";
    }

    VulkanRHIShader::~VulkanRHIShader() = default;
#endif

#if defined(MINENGINE_HAS_VULKAN)
    VulkanRHIBuffer::VulkanRHIBuffer(
        const VulkanDeviceContext& context,
        const RHIBufferCreateDesc& desc,
        const void* initialData)
        : m_Desc(desc)
        , m_Context(context)
    {
        if (!m_Context.IsValid() || m_Desc.ByteSize == 0)
        {
            ME_CORE_ERROR("VulkanRHIBuffer: invalid device context or zero ByteSize.");
            return;
        }

        // S07a: host-visible buffers so UpdateSubresource works without a frame submit.
        // DEVICE_LOCAL + staging can follow when draw paths need bandwidth.
        const VkBufferUsageFlags usageFlags = [&]() -> VkBufferUsageFlags
        {
            switch (m_Desc.Usage)
            {
            case RHIBufferUsage::Vertex:
                return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            case RHIBufferUsage::Index:
                return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            case RHIBufferUsage::Uniform:
                return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            case RHIBufferUsage::Staging:
                return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            default:
                return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            }
        }();

        if (!VulkanRHIAllocator::CreateBuffer(
                m_Context,
                m_Desc.ByteSize,
                usageFlags,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                m_Buffer,
                m_Memory))
        {
            return;
        }

        if (vkMapMemory(m_Context.Device, m_Memory, 0, m_Desc.ByteSize, 0, &m_Mapped) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIBuffer: vkMapMemory failed.");
            VulkanRHIAllocator::DestroyBuffer(m_Context.Device, m_Buffer, m_Memory);
            m_Mapped = nullptr;
            return;
        }

        if (initialData != nullptr)
        {
            std::memcpy(m_Mapped, initialData, m_Desc.ByteSize);
        }
        else
        {
            std::memset(m_Mapped, 0, m_Desc.ByteSize);
        }

        m_IsValid = true;
    }

    VulkanRHIBuffer::~VulkanRHIBuffer()
    {
        if (m_Context.Device == VK_NULL_HANDLE)
        {
            return;
        }
        if (m_Mapped != nullptr)
        {
            vkUnmapMemory(m_Context.Device, m_Memory);
            m_Mapped = nullptr;
        }
        if (m_Context.OwnerRHI != nullptr)
        {
            m_Context.OwnerRHI->RetireBuffer(m_Buffer, m_Memory);
            m_Buffer = VK_NULL_HANDLE;
            m_Memory = VK_NULL_HANDLE;
            return;
        }
        VulkanRHIAllocator::DestroyBuffer(m_Context.Device, m_Buffer, m_Memory);
    }

    void VulkanRHIBuffer::UpdateSubresource(const void* data, uint32_t offset, uint32_t size)
    {
        if (!m_IsValid || data == nullptr || size == 0 || m_Mapped == nullptr)
        {
            return;
        }
        if (offset + size > m_Desc.ByteSize)
        {
            ME_CORE_ERROR(
                "VulkanRHIBuffer::UpdateSubresource out of range (offset={}, size={}, capacity={}).",
                offset,
                size,
                m_Desc.ByteSize);
            return;
        }
        std::memcpy(static_cast<uint8_t*>(m_Mapped) + offset, data, size);
    }
#else
    VulkanRHIBuffer::~VulkanRHIBuffer() = default;

    void VulkanRHIBuffer::UpdateSubresource(const void* data, uint32_t offset, uint32_t size)
    {
        (void)data;
        (void)offset;
        (void)size;
    }
#endif

#if defined(MINENGINE_HAS_VULKAN)
    VulkanRHITexture::VulkanRHITexture(
        const VulkanDeviceContext& context,
        const RHITextureCreateDesc& desc,
        const void* initialData)
        : m_Desc(desc)
        , m_Context(context)
    {
        if (!m_Context.IsValid())
        {
            ME_CORE_ERROR("VulkanRHITexture: invalid device context.");
            return;
        }
        if (m_Desc.Dimension != RHITextureDimension::Texture2D &&
            m_Desc.Dimension != RHITextureDimension::TextureCube)
        {
            ME_CORE_ERROR(
                "VulkanRHITexture: unsupported dimension {}.",
                static_cast<int>(m_Desc.Dimension));
            return;
        }
        if (m_Desc.Width == 0 || m_Desc.Height == 0)
        {
            ME_CORE_ERROR("VulkanRHITexture: Width/Height must be > 0.");
            return;
        }

        const bool isCube = m_Desc.Dimension == RHITextureDimension::TextureCube;
        if (isCube && m_Desc.Width != m_Desc.Height)
        {
            ME_CORE_WARN(
                "VulkanRHITexture: cube faces should be square (got {}x{}).",
                m_Desc.Width,
                m_Desc.Height);
        }

        m_VkFormat = VulkanRHIAllocator::ToVkFormat(m_Desc.Format);
        if (m_VkFormat == VK_FORMAT_UNDEFINED)
        {
            ME_CORE_ERROR("VulkanRHITexture: unsupported TextureFormat.");
            return;
        }

        const uint32_t mipLevels = m_Desc.NumMips == 0 ? 1u : m_Desc.NumMips;
        m_Desc.NumMips = mipLevels;

        const VkImageAspectFlags aspect = VulkanRHIAllocator::AspectFromFormat(m_Desc.Format);
        const bool isDepth = (aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;

        VkImageUsageFlags usage = 0;
        if (HasTextureCreateFlag(m_Desc.Flags, RHITextureCreateFlags::RenderTarget))
        {
            usage |= isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (HasTextureCreateFlag(m_Desc.Flags, RHITextureCreateFlags::ShaderResource))
        {
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if (initialData != nullptr && !isDepth)
        {
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        if (usage == 0)
        {
            ME_CORE_ERROR("VulkanRHITexture: no usage flags derived from create desc.");
            return;
        }

        if (isCube)
        {
            if (!VulkanRHIAllocator::CreateImageCube(
                    m_Context,
                    m_Desc.Width,
                    mipLevels,
                    m_VkFormat,
                    usage,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    m_Image,
                    m_Memory))
            {
                return;
            }

            if (!VulkanRHIAllocator::CreateImageViewCube(
                    m_Context.Device,
                    m_Image,
                    m_VkFormat,
                    aspect,
                    mipLevels,
                    m_ImageView))
            {
                VulkanRHIAllocator::DestroyImage(m_Context.Device, m_Image, m_Memory);
                return;
            }
        }
        else
        {
            if (!VulkanRHIAllocator::CreateImage2D(
                    m_Context,
                    m_Desc.Width,
                    m_Desc.Height,
                    mipLevels,
                    m_VkFormat,
                    usage,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    m_Image,
                    m_Memory))
            {
                return;
            }

            if (!VulkanRHIAllocator::CreateImageView2D(
                    m_Context.Device,
                    m_Image,
                    m_VkFormat,
                    aspect,
                    mipLevels,
                    m_ImageView))
            {
                VulkanRHIAllocator::DestroyImage(m_Context.Device, m_Image, m_Memory);
                return;
            }
        }

        if (initialData != nullptr && !isDepth)
        {
            const uint32_t srcBpp = VulkanRHIAllocator::BytesPerPixel(m_Desc.Format);
            if (srcBpp == 0)
            {
                ME_CORE_ERROR("VulkanRHITexture: cannot upload initialData for this format.");
            }
            else if (isCube)
            {
                const auto* facePtrs = static_cast<const unsigned char* const*>(initialData);
                const bool isFloat16Color =
                    m_Desc.Format == TextureFormat::RGB16F || m_Desc.Format == TextureFormat::RGBA16F;
                const bool expandRgb8 = m_Desc.Format == TextureFormat::RGB8;
                const uint32_t dstBpp =
                    isFloat16Color ? 8u : (expandRgb8 ? (srcBpp + srcBpp / 3u) : srcBpp);
                const VkDeviceSize faceBytes =
                    static_cast<VkDeviceSize>(m_Desc.Width) * m_Desc.Height * dstBpp;
                const VkDeviceSize stagingBytes = faceBytes * 6;

                VkBuffer stagingBuffer = VK_NULL_HANDLE;
                VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
                if (VulkanRHIAllocator::CreateBuffer(
                        m_Context,
                        stagingBytes,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        stagingBuffer,
                        stagingMemory))
                {
                    void* mapped = nullptr;
                    if (vkMapMemory(m_Context.Device, stagingMemory, 0, stagingBytes, 0, &mapped) ==
                        VK_SUCCESS)
                    {
                        for (uint32_t face = 0; face < 6; ++face)
                        {
                            const unsigned char* facePixels =
                                facePtrs != nullptr ? facePtrs[face] : nullptr;
                            if (facePixels == nullptr)
                            {
                                continue;
                            }

                            auto* dstBase = static_cast<uint8_t*>(mapped) + faceBytes * face;
                            const uint32_t pixelCount = m_Desc.Width * m_Desc.Height;
                            if (m_Desc.Format == TextureFormat::RGB16F)
                            {
                                PackFloatRgbToHalfRgba(
                                    reinterpret_cast<const float*>(facePixels),
                                    reinterpret_cast<uint16_t*>(dstBase),
                                    pixelCount);
                            }
                            else if (m_Desc.Format == TextureFormat::RGBA16F)
                            {
                                PackFloatRgbaToHalfRgba(
                                    reinterpret_cast<const float*>(facePixels),
                                    reinterpret_cast<uint16_t*>(dstBase),
                                    pixelCount);
                            }
                            else if (expandRgb8)
                            {
                                for (uint32_t i = 0; i < pixelCount; ++i)
                                {
                                    dstBase[i * 4 + 0] = facePixels[i * 3 + 0];
                                    dstBase[i * 4 + 1] = facePixels[i * 3 + 1];
                                    dstBase[i * 4 + 2] = facePixels[i * 3 + 2];
                                    dstBase[i * 4 + 3] = 255;
                                }
                            }
                            else
                            {
                                std::memcpy(dstBase, facePixels, static_cast<size_t>(faceBytes));
                            }
                        }
                        vkUnmapMemory(m_Context.Device, stagingMemory);

                        if (!VulkanRHIAllocator::UploadBufferToImageCube(
                                m_Context,
                                stagingBuffer,
                                m_Image,
                                m_Desc.Width,
                                faceBytes,
                                aspect))
                        {
                            ME_CORE_ERROR("VulkanRHITexture: cube initial upload failed.");
                        }
                        else
                        {
                            m_CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        }
                    }
                    VulkanRHIAllocator::DestroyBuffer(m_Context.Device, stagingBuffer, stagingMemory);
                }
            }
            else
            {
                // 2D upload. TextureFormat::*16F CPU pixels are float32 (same contract as OpenGL GL_FLOAT).
                const bool isFloat16Color =
                    m_Desc.Format == TextureFormat::RGB16F || m_Desc.Format == TextureFormat::RGBA16F;
                const bool expandRgb8 = m_Desc.Format == TextureFormat::RGB8;
                const uint32_t dstBpp =
                    isFloat16Color ? 8u : (expandRgb8 ? (srcBpp + srcBpp / 3u) : srcBpp);
                const VkDeviceSize imageBytes =
                    static_cast<VkDeviceSize>(m_Desc.Width) * m_Desc.Height * dstBpp;

                VkBuffer stagingBuffer = VK_NULL_HANDLE;
                VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
                if (VulkanRHIAllocator::CreateBuffer(
                        m_Context,
                        imageBytes,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        stagingBuffer,
                        stagingMemory))
                {
                    void* mapped = nullptr;
                    if (vkMapMemory(m_Context.Device, stagingMemory, 0, imageBytes, 0, &mapped) == VK_SUCCESS)
                    {
                        const uint32_t pixelCount = m_Desc.Width * m_Desc.Height;
                        if (m_Desc.Format == TextureFormat::RGB16F)
                        {
                            PackFloatRgbToHalfRgba(
                                static_cast<const float*>(initialData),
                                static_cast<uint16_t*>(mapped),
                                pixelCount);
                        }
                        else if (m_Desc.Format == TextureFormat::RGBA16F)
                        {
                            PackFloatRgbaToHalfRgba(
                                static_cast<const float*>(initialData),
                                static_cast<uint16_t*>(mapped),
                                pixelCount);
                        }
                        else if (expandRgb8)
                        {
                            const auto* src = static_cast<const uint8_t*>(initialData);
                            auto* dst = static_cast<uint8_t*>(mapped);
                            for (uint32_t i = 0; i < pixelCount; ++i)
                            {
                                dst[i * 4 + 0] = src[i * 3 + 0];
                                dst[i * 4 + 1] = src[i * 3 + 1];
                                dst[i * 4 + 2] = src[i * 3 + 2];
                                dst[i * 4 + 3] = 255;
                            }
                        }
                        else
                        {
                            std::memcpy(mapped, initialData, static_cast<size_t>(imageBytes));
                        }
                        vkUnmapMemory(m_Context.Device, stagingMemory);

                        if (!VulkanRHIAllocator::UploadBufferToImage2D(
                                m_Context,
                                stagingBuffer,
                                m_Image,
                                m_Desc.Width,
                                m_Desc.Height,
                                aspect))
                        {
                            ME_CORE_ERROR("VulkanRHITexture: initial upload failed.");
                        }
                        else
                        {
                            m_CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        }
                    }
                    VulkanRHIAllocator::DestroyBuffer(m_Context.Device, stagingBuffer, stagingMemory);
                }
            }
        }

        m_IsValid = true;
    }

    VulkanRHITexture::~VulkanRHITexture()
    {
        if (m_Context.Device == VK_NULL_HANDLE)
        {
            return;
        }
        if (m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Context.Device, m_ImageView, nullptr);
            m_ImageView = VK_NULL_HANDLE;
        }
        VulkanRHIAllocator::DestroyImage(m_Context.Device, m_Image, m_Memory);
    }

    void* VulkanRHITexture::GetNativeResource() const
    {
        return reinterpret_cast<void*>(m_Image);
    }

    VulkanRHIShaderResourceView::VulkanRHIShaderResourceView(VkDevice device, const RHITextureSRVDesc& desc)
        : m_Desc(desc)
        , m_Device(device)
    {
        if (m_Desc.Texture == nullptr)
        {
            ME_CORE_ERROR("VulkanRHIShaderResourceView: Texture is null.");
            return;
        }

        auto* vulkanTexture = dynamic_cast<VulkanRHITexture*>(m_Desc.Texture);
        if (vulkanTexture == nullptr || !vulkanTexture->IsValid())
        {
            ME_CORE_ERROR("VulkanRHIShaderResourceView: Texture is not a valid VulkanRHITexture.");
            return;
        }

        // S07a: reuse the texture's default full-range view when MipIndex==0 and no array slice.
        // Dedicated per-mip views come with richer descriptor work in S07b+.
        if (m_Desc.MipIndex == 0 && m_Desc.ArraySlice < 0)
        {
            m_ImageView = vulkanTexture->GetImageView();
            m_OwnsView = false;
            m_IsValid = m_ImageView != VK_NULL_HANDLE;
            return;
        }

        const RHITextureCreateDesc& texDesc = vulkanTexture->GetDesc();
        const VkImageAspectFlags aspect = VulkanRHIAllocator::AspectFromFormat(texDesc.Format);
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vulkanTexture->GetImage();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = vulkanTexture->GetVkFormat();
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = m_Desc.MipIndex;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = m_Desc.ArraySlice >= 0 ? static_cast<uint32_t>(m_Desc.ArraySlice) : 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIShaderResourceView: vkCreateImageView failed.");
            m_ImageView = VK_NULL_HANDLE;
            return;
        }
        m_OwnsView = true;
        m_IsValid = true;
    }

    VulkanRHIShaderResourceView::~VulkanRHIShaderResourceView()
    {
        if (m_OwnsView && m_Device != VK_NULL_HANDLE && m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Device, m_ImageView, nullptr);
            m_ImageView = VK_NULL_HANDLE;
        }
    }
#else
    void* VulkanRHITexture::GetNativeResource() const
    {
        return nullptr;
    }

    VulkanRHITexture::~VulkanRHITexture() = default;

    VulkanRHIShaderResourceView::VulkanRHIShaderResourceView(const RHITextureSRVDesc& desc)
        : m_Desc(desc)
    {
    }

    VulkanRHIShaderResourceView::~VulkanRHIShaderResourceView() = default;
#endif

    VulkanRHIVertexInputLayout::VulkanRHIVertexInputLayout(std::initializer_list<RHIVertexElement> elements)
    {
        uint32_t offset = 0;
        for (const RHIVertexElement& element : elements)
        {
            RHIVertexElement copy = element;
            copy.Offset = offset;
            offset += VertexElementTypeSize(element.Type);
            m_Elements.push_back(copy);
        }
        m_Stride = offset;
    }

#if defined(MINENGINE_HAS_VULKAN)
    VulkanRHIShaderBindingSetLayout::VulkanRHIShaderBindingSetLayout(
        VkDevice device,
        std::vector<RHIShaderBindingSetLayoutEntry> entries)
        : m_Device(device)
        , m_Entries(std::move(entries))
    {
        if (m_Device == VK_NULL_HANDLE)
        {
            ME_CORE_ERROR("VulkanRHIShaderBindingSetLayout: device is null.");
            return;
        }

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(m_Entries.size());
        for (const RHIShaderBindingSetLayoutEntry& entry : m_Entries)
        {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = entry.Slot;
            binding.descriptorCount = 1;
            if (entry.Visibility == RHIGraphicsShaderStage::Vertex)
            {
                binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            }
            else if (entry.Visibility == RHIGraphicsShaderStage::All)
            {
                binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            else
            {
                binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            if (entry.Type == RHIShaderBindingType::UniformBuffer)
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
            else
            {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.empty() ? nullptr : bindings.data();
        if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_Layout) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIShaderBindingSetLayout: vkCreateDescriptorSetLayout failed.");
            m_Layout = VK_NULL_HANDLE;
        }
    }

    VulkanRHIShaderBindingSetLayout::~VulkanRHIShaderBindingSetLayout()
    {
        if (m_Device != VK_NULL_HANDLE && m_Layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(m_Device, m_Layout, nullptr);
            m_Layout = VK_NULL_HANDLE;
        }
    }

    VulkanRHIPipelineLayout::VulkanRHIPipelineLayout(
        VkDevice device,
        std::vector<RHIShaderBindingSetLayout*> setLayouts)
        : m_Device(device)
        , m_SetLayouts(std::move(setLayouts))
    {
        if (m_Device == VK_NULL_HANDLE)
        {
            ME_CORE_ERROR("VulkanRHIPipelineLayout: device is null.");
            return;
        }

        std::vector<VkDescriptorSetLayout> vkLayouts;
        vkLayouts.reserve(m_SetLayouts.size());
        for (RHIShaderBindingSetLayout* setLayout : m_SetLayouts)
        {
            auto* vulkanLayout = dynamic_cast<VulkanRHIShaderBindingSetLayout*>(setLayout);
            if (vulkanLayout == nullptr || !vulkanLayout->IsValid())
            {
                ME_CORE_ERROR("VulkanRHIPipelineLayout: set layout is invalid.");
                return;
            }
            vkLayouts.push_back(vulkanLayout->GetVkLayout());
        }

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = static_cast<uint32_t>(vkLayouts.size());
        layoutInfo.pSetLayouts = vkLayouts.empty() ? nullptr : vkLayouts.data();
        if (vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIPipelineLayout: vkCreatePipelineLayout failed.");
            m_PipelineLayout = VK_NULL_HANDLE;
        }
    }

    VulkanRHIPipelineLayout::~VulkanRHIPipelineLayout()
    {
        if (m_Device != VK_NULL_HANDLE && m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
            m_PipelineLayout = VK_NULL_HANDLE;
        }
    }

    RHIShaderBindingSetLayout* VulkanRHIPipelineLayout::GetShaderBindingSetLayout(uint32_t setIndex) const
    {
        if (setIndex >= m_SetLayouts.size())
        {
            return nullptr;
        }
        return m_SetLayouts[setIndex];
    }

    VulkanRHIShaderBindingSet::VulkanRHIShaderBindingSet(
        VkDevice device,
        VkDescriptorPool pool,
        VkSampler defaultSampler,
        VkBuffer dummyUniformBuffer,
        const DummyImageViews& dummyImageViews,
        RHIShaderBindingSetLayout* layout,
        std::vector<RHIShaderBinding> resources)
        : m_Device(device)
        , m_Pool(pool)
        , m_Layout(layout)
        , m_Resources(std::move(resources))
    {
        using namespace EngineShaderBindings;

        auto selectDummyImageView = [&](const RHIShaderBindingSetLayoutEntry& entry) -> VkImageView
        {
            if (entry.Type != RHIShaderBindingType::TextureSRV)
            {
                return dummyImageViews.Image2D;
            }

            switch (entry.Slot)
            {
            case kSet1_DirShadowSRV:
                return dummyImageViews.Image2DArray != VK_NULL_HANDLE ? dummyImageViews.Image2DArray
                                                                      : dummyImageViews.Image2D;
            case kSet1_PointShadow0:
            case kSet1_PointShadow1:
            case kSet1_IBLIrradiance:
            case kSet1_IBLPrefilter:
                return dummyImageViews.ImageCube != VK_NULL_HANDLE ? dummyImageViews.ImageCube
                                                                   : dummyImageViews.Image2D;
            default:
                return dummyImageViews.Image2D;
            }
        };

        auto* vulkanLayout = dynamic_cast<VulkanRHIShaderBindingSetLayout*>(layout);
        if (m_Device == VK_NULL_HANDLE || m_Pool == VK_NULL_HANDLE || vulkanLayout == nullptr ||
            !vulkanLayout->IsValid())
        {
            ME_CORE_ERROR("VulkanRHIShaderBindingSet: invalid device/pool/layout.");
            return;
        }

        VkDescriptorSetLayout setLayout = vulkanLayout->GetVkLayout();
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_Pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &setLayout;
        if (vkAllocateDescriptorSets(m_Device, &allocInfo, &m_DescriptorSet) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIShaderBindingSet: vkAllocateDescriptorSets failed.");
            m_DescriptorSet = VK_NULL_HANDLE;
            return;
        }

        const std::vector<RHIShaderBindingSetLayoutEntry>& entries = vulkanLayout->GetEntries();
        const size_t writeCount = std::min(entries.size(), m_Resources.size());

        std::vector<VkDescriptorBufferInfo> bufferInfos(writeCount);
        std::vector<VkDescriptorImageInfo> imageInfos(writeCount);
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(writeCount);

        for (size_t i = 0; i < writeCount; ++i)
        {
            const RHIShaderBindingSetLayoutEntry& entry = entries[i];
            const RHIShaderBinding& resource = m_Resources[i];

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_DescriptorSet;
            write.dstBinding = entry.Slot;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;

            if (entry.Type == RHIShaderBindingType::UniformBuffer)
            {
                VkBuffer buffer = dummyUniformBuffer;
                VkDeviceSize range = VK_WHOLE_SIZE;
                VkDeviceSize offset = 0;
                if (resource.Buffer != nullptr)
                {
                    if (auto* vulkanBuffer = dynamic_cast<VulkanRHIBuffer*>(resource.Buffer))
                    {
                        buffer = vulkanBuffer->GetBuffer();
                        offset = resource.BufferOffset;
                        range = resource.BufferRange != 0
                            ? resource.BufferRange
                            : vulkanBuffer->GetDesc().ByteSize;
                    }
                }

                bufferInfos[i].buffer = buffer;
                bufferInfos[i].offset = offset;
                bufferInfos[i].range = range == 0 ? VK_WHOLE_SIZE : range;

                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.pBufferInfo = &bufferInfos[i];
            }
            else
            {
                VkImageView imageView = selectDummyImageView(entry);
                if (resource.TextureSRV != nullptr)
                {
                    if (auto* vulkanSrv = dynamic_cast<VulkanRHIShaderResourceView*>(resource.TextureSRV))
                    {
                        if (vulkanSrv->IsValid())
                        {
                            imageView = vulkanSrv->GetImageView();
                        }
                    }
                }

                imageInfos[i].sampler = defaultSampler;
                imageInfos[i].imageView = imageView;
                imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pImageInfo = &imageInfos[i];
            }

            writes.push_back(write);
        }

        if (!writes.empty())
        {
            vkUpdateDescriptorSets(
                m_Device,
                static_cast<uint32_t>(writes.size()),
                writes.data(),
                0,
                nullptr);
        }
    }

    VulkanRHIShaderBindingSet::~VulkanRHIShaderBindingSet()
    {
        if (m_Device != VK_NULL_HANDLE && m_Pool != VK_NULL_HANDLE && m_DescriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(m_Device, m_Pool, 1, &m_DescriptorSet);
            m_DescriptorSet = VK_NULL_HANDLE;
        }
    }

    VulkanRHIGraphicsPipelineState::VulkanRHIGraphicsPipelineState(VkDevice device, const RHIGraphicsPSODesc& desc)
        : RHIGraphicsPSOStateFallback(desc)
        , m_Device(device)
    {
    }

    VulkanRHIGraphicsPipelineState::~VulkanRHIGraphicsPipelineState()
    {
        if (m_Device == VK_NULL_HANDLE)
        {
            return;
        }
        for (auto& pair : m_PipelinesByRenderPass)
        {
            if (pair.second != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(m_Device, pair.second, nullptr);
            }
        }
        m_PipelinesByRenderPass.clear();
    }

    VkPipelineLayout VulkanRHIGraphicsPipelineState::GetVkPipelineLayout() const
    {
        auto* vulkanLayout = dynamic_cast<VulkanRHIPipelineLayout*>(GetDesc().PipelineLayout);
        return vulkanLayout != nullptr ? vulkanLayout->GetVkPipelineLayout() : VK_NULL_HANDLE;
    }

    VkFormat VulkanRHIGraphicsPipelineState::ToVkVertexFormat(VertexElementType type)
    {
        switch (type)
        {
        case VertexElementType::Float:
            return VK_FORMAT_R32_SFLOAT;
        case VertexElementType::Float2:
            return VK_FORMAT_R32G32_SFLOAT;
        case VertexElementType::Float3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexElementType::Float4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexElementType::Int:
            return VK_FORMAT_R32_SINT;
        case VertexElementType::Int2:
            return VK_FORMAT_R32G32_SINT;
        case VertexElementType::Int3:
            return VK_FORMAT_R32G32B32_SINT;
        case VertexElementType::Int4:
            return VK_FORMAT_R32G32B32A32_SINT;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }

    VkPrimitiveTopology VulkanRHIGraphicsPipelineState::ToVkTopology(RHIPrimitiveType type)
    {
        switch (type)
        {
        case RHIPrimitiveType::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case RHIPrimitiveType::LineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case RHIPrimitiveType::TriangleList:
        default:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    VkCompareOp VulkanRHIGraphicsPipelineState::ToVkCompareOp(RHIDepthCompareFunc func)
    {
        switch (func)
        {
        case RHIDepthCompareFunc::LessEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RHIDepthCompareFunc::Always:
            return VK_COMPARE_OP_ALWAYS;
        case RHIDepthCompareFunc::Less:
        default:
            return VK_COMPARE_OP_LESS;
        }
    }

    VkPipeline VulkanRHIGraphicsPipelineState::GetOrCreatePipeline(VkRenderPass renderPass)
    {
        if (m_Device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE)
        {
            return VK_NULL_HANDLE;
        }

        const auto existing = m_PipelinesByRenderPass.find(renderPass);
        if (existing != m_PipelinesByRenderPass.end())
        {
            return existing->second;
        }

        const RHIGraphicsPSODesc& desc = GetDesc();
        auto* vertexShader = dynamic_cast<VulkanRHIShader*>(desc.VertexShader);
        auto* pixelShader = dynamic_cast<VulkanRHIShader*>(desc.PixelShader);
        if (vertexShader == nullptr || pixelShader == nullptr || !vertexShader->IsValid() || !pixelShader->IsValid())
        {
            ME_CORE_ERROR("VulkanRHIGraphicsPipelineState: invalid shader modules.");
            return VK_NULL_HANDLE;
        }

        VkPipelineLayout pipelineLayout = GetVkPipelineLayout();
        if (pipelineLayout == VK_NULL_HANDLE)
        {
            ME_CORE_ERROR("VulkanRHIGraphicsPipelineState: PipelineLayout is null.");
            return VK_NULL_HANDLE;
        }

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertexShader->GetVertexModule();
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = pixelShader->GetFragmentModule();
        fragStage.pName = "main";

        const VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributes;
        if (auto* vertexLayout = dynamic_cast<VulkanRHIVertexInputLayout*>(desc.VertexInputLayout))
        {
            bindingDesc.stride = vertexLayout->GetStride();
            const std::vector<RHIVertexElement>& elements = vertexLayout->GetElements();
            attributes.reserve(elements.size());
            for (uint32_t i = 0; i < elements.size(); ++i)
            {
                VkVertexInputAttributeDescription attribute{};
                attribute.location = i;
                attribute.binding = 0;
                attribute.format = ToVkVertexFormat(elements[i].Type);
                attribute.offset = elements[i].Offset;
                attributes.push_back(attribute);
            }
        }

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        if (bindingDesc.stride > 0)
        {
            vertexInput.vertexBindingDescriptionCount = 1;
            vertexInput.pVertexBindingDescriptions = &bindingDesc;
            vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
            vertexInput.pVertexAttributeDescriptions = attributes.data();
        }

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = ToVkTopology(desc.PrimitiveType);
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        if (!desc.RasterizerState.bCullEnabled || desc.RasterizerState.CullMode == RHICullMode::None)
        {
            rasterizer.cullMode = VK_CULL_MODE_NONE;
        }
        else if (desc.RasterizerState.CullMode == RHICullMode::Front)
        {
            rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        }
        else
        {
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        }
        rasterizer.depthBiasEnable =
            desc.RasterizerState.DepthBiasSlopeScale != 0.0f || desc.RasterizerState.DepthBiasConstant != 0.0f;
        rasterizer.depthBiasConstantFactor = desc.RasterizerState.DepthBiasConstant;
        rasterizer.depthBiasSlopeFactor = desc.RasterizerState.DepthBiasSlopeScale;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = desc.DepthStencilState.bDepthTestEnabled ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = desc.DepthStencilState.bDepthWriteEnabled ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = ToVkCompareOp(desc.DepthStencilState.DepthCompare);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = desc.BlendState.bBlendEnabled ? VK_TRUE : VK_FALSE;
        if (desc.BlendState.bBlendEnabled)
        {
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        const bool hasColorTarget = desc.RenderTargetsEnabled > 0 || desc.RenderTargetFormats[0] != TextureFormat::None;
        // Present / swapchain PSOs often leave formats unset; still need one color attachment.
        const bool expectColorAttachment =
            hasColorTarget || desc.DepthStencilTargetFormat == TextureFormat::None;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        if (expectColorAttachment)
        {
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
        }
        else
        {
            colorBlending.attachmentCount = 0;
            colorBlending.pAttachments = nullptr;
        }

        const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        VkPipeline pipeline = VK_NULL_HANDLE;
        if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
        {
            ME_CORE_ERROR("VulkanRHIGraphicsPipelineState: vkCreateGraphicsPipelines failed.");
            return VK_NULL_HANDLE;
        }

        m_PipelinesByRenderPass.emplace(renderPass, pipeline);
        return pipeline;
    }
#endif
}
