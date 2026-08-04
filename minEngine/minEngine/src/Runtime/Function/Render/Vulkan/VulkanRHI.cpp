#include "VulkanRHI.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <vector>

#if defined(MINENGINE_HAS_VULKAN)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

extern "C"
{
    GLFWAPI VkResult glfwCreateWindowSurface(
        VkInstance instance,
        GLFWwindow* window,
        const VkAllocationCallbacks* allocator,
        VkSurfaceKHR* surface);
    GLFWAPI const char** glfwGetRequiredInstanceExtensions(uint32_t* count);
}
#endif

namespace minEngine
{
#if defined(MINENGINE_HAS_VULKAN)
    namespace
    {
        const char* VkResultToString(VkResult result)
        {
            switch (result)
            {
            case VK_SUCCESS:
                return "VK_SUCCESS";
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED:
                return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT:
                return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_SURFACE_LOST_KHR:
                return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
                return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_ERROR_DEVICE_LOST:
                return "VK_ERROR_DEVICE_LOST";
            default:
                return "VK_ERROR";
            }
        }

        bool CheckVk(VkResult result, const char* what)
        {
            if (result == VK_SUCCESS)
            {
                return true;
            }
            ME_CORE_ERROR("VulkanRHI: {} failed ({})", what, VkResultToString(result));
            return false;
        }
    }
#endif

    VulkanRHI::~VulkanRHI()
    {
        Shutdown();
    }

    void VulkanRHI::Initialize()
    {
#if !defined(MINENGINE_HAS_VULKAN)
        ME_CORE_ERROR("VulkanRHI: built without MINENGINE_HAS_VULKAN (Vulkan SDK / CMake link required).");
        return;
#else
        if (m_Initialized)
        {
            return;
        }

        if (!CreateInstance() || !CreateSurface() || !PickPhysicalDevice() || !CreateDevice() ||
            !CreateSwapchain() || !CreateCommandResources() || !CreateSyncObjects())
        {
            Shutdown();
            ME_CORE_ERROR("VulkanRHI: Initialize failed.");
            return;
        }

        m_Initialized = true;
        ME_CORE_INFO(
            "VulkanRHI Initialized (swapchain {}x{}, format={})",
            m_SwapchainExtent.width,
            m_SwapchainExtent.height,
            static_cast<int>(m_SwapchainFormat));
#endif
    }

    void VulkanRHI::Shutdown()
    {
#if defined(MINENGINE_HAS_VULKAN)
        if (m_Device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(m_Device);
        }

        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            if (m_InFlightFences[i] != VK_NULL_HANDLE)
            {
                vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
                m_InFlightFences[i] = VK_NULL_HANDLE;
            }
            if (m_RenderFinishedSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
                m_RenderFinishedSemaphores[i] = VK_NULL_HANDLE;
            }
            if (m_ImageAvailableSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
                m_ImageAvailableSemaphores[i] = VK_NULL_HANDLE;
            }
        }

        if (m_CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
            m_CommandPool = VK_NULL_HANDLE;
        }

        DestroySwapchain();

        if (m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_Device, nullptr);
            m_Device = VK_NULL_HANDLE;
        }

        if (m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        if (m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
        }

        m_PhysicalDevice = VK_NULL_HANDLE;
        m_GraphicsQueue = VK_NULL_HANDLE;
        m_FramePrepared = false;
        m_Initialized = false;
        ME_CORE_INFO("VulkanRHI Shutdown");
#endif
    }

#if defined(MINENGINE_HAS_VULKAN)
    bool VulkanRHI::CreateInstance()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (glfwExtensions == nullptr || glfwExtensionCount == 0)
        {
            ME_CORE_ERROR("VulkanRHI: glfwGetRequiredInstanceExtensions failed.");
            return false;
        }

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "minEngine";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "minEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        return CheckVk(vkCreateInstance(&createInfo, nullptr, &m_Instance), "vkCreateInstance");
    }

    bool VulkanRHI::CreateSurface()
    {
        auto* glfwWindow = static_cast<GLFWWindowSystem*>(&WindowSystem::Get());
        GLFWwindow* window = static_cast<GLFWwindow*>(glfwWindow->GetWindowHandle());
        if (window == nullptr)
        {
            ME_CORE_ERROR("VulkanRHI: GLFW window handle is null.");
            return false;
        }

        return CheckVk(
            glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface),
            "glfwCreateWindowSurface");
    }

    bool VulkanRHI::PickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            ME_CORE_ERROR("VulkanRHI: no physical devices.");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        for (VkPhysicalDevice candidate : devices)
        {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());

            for (uint32_t i = 0; i < queueFamilyCount; ++i)
            {
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, m_Surface, &presentSupport);
                if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
                {
                    m_PhysicalDevice = candidate;
                    m_GraphicsQueueFamily = i;
                    return true;
                }
            }
        }

        ME_CORE_ERROR("VulkanRHI: no suitable graphics+present queue family.");
        return false;
    }

    bool VulkanRHI::CreateDevice()
    {
        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = deviceExtensions;

        if (!CheckVk(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device), "vkCreateDevice"))
        {
            return false;
        }

        vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
        return true;
    }

    bool VulkanRHI::CreateSwapchain()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());
        if (formats.empty())
        {
            ME_CORE_ERROR("VulkanRHI: no surface formats.");
            return false;
        }

        VkSurfaceFormatKHR chosenFormat = formats[0];
        for (const VkSurfaceFormatKHR& format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosenFormat = format;
                break;
            }
        }
        m_SwapchainFormat = chosenFormat.format;

        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            m_SwapchainExtent = capabilities.currentExtent;
        }
        else
        {
            auto& window = WindowSystem::Get();
            m_SwapchainExtent.width = std::clamp(
                window.GetWidth(),
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width);
            m_SwapchainExtent.height = std::clamp(
                window.GetHeight(),
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = chosenFormat.format;
        createInfo.imageColorSpace = chosenFormat.colorSpace;
        createInfo.imageExtent = m_SwapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;

        if (!CheckVk(vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain), "vkCreateSwapchainKHR"))
        {
            return false;
        }

        uint32_t swapImageCount = 0;
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &swapImageCount, nullptr);
        m_SwapchainImages.resize(swapImageCount);
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &swapImageCount, m_SwapchainImages.data());

        m_SwapchainImageViews.resize(swapImageCount);
        for (uint32_t i = 0; i < swapImageCount; ++i)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_SwapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_SwapchainFormat;
            viewInfo.components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY};
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            if (!CheckVk(
                    vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SwapchainImageViews[i]),
                    "vkCreateImageView"))
            {
                return false;
            }
        }

        return true;
    }

    void VulkanRHI::DestroySwapchain()
    {
        for (VkImageView view : m_SwapchainImageViews)
        {
            if (view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_Device, view, nullptr);
            }
        }
        m_SwapchainImageViews.clear();
        m_SwapchainImages.clear();

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }

    void VulkanRHI::RecreateSwapchain()
    {
        vkDeviceWaitIdle(m_Device);
        DestroySwapchain();
        CreateSwapchain();
        m_FramePrepared = false;
    }

    bool VulkanRHI::CreateCommandResources()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_GraphicsQueueFamily;
        if (!CheckVk(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool), "vkCreateCommandPool"))
        {
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = kMaxFramesInFlight;
        return CheckVk(
            vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()),
            "vkAllocateCommandBuffers");
    }

    bool VulkanRHI::CreateSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            if (!CheckVk(
                    vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]),
                    "vkCreateSemaphore(imageAvailable)") ||
                !CheckVk(
                    vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]),
                    "vkCreateSemaphore(renderFinished)") ||
                !CheckVk(
                    vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]),
                    "vkCreateFence"))
            {
                return false;
            }
        }
        return true;
    }

    bool VulkanRHI::RecordClearCommands(uint32_t imageIndex)
    {
        VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (!CheckVk(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer"))
        {
            return false;
        }

        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = m_SwapchainImages[imageIndex];
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer);

        VkClearColorValue clearColor{};
        clearColor.float32[0] = m_ClearColor.x;
        clearColor.float32[1] = m_ClearColor.y;
        clearColor.float32[2] = m_ClearColor.z;
        clearColor.float32[3] = 1.0f;
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;
        vkCmdClearColorImage(
            cmd,
            m_SwapchainImages[imageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearColor,
            1,
            &range);

        VkImageMemoryBarrier toPresent = toTransfer;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toPresent.dstAccessMask = 0;
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toPresent);

        return CheckVk(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
    }
#endif

    void VulkanRHI::RHISetBackbufferClearColor(const Vector3& color)
    {
        m_ClearColor = color;
    }

    void VulkanRHI::RHIClearBackbuffer()
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)0;
#else
        if (!m_Initialized)
        {
            return;
        }

        vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        VkResult acquireResult = vkAcquireNextImageKHR(
            m_Device,
            m_Swapchain,
            UINT64_MAX,
            m_ImageAvailableSemaphores[m_CurrentFrame],
            VK_NULL_HANDLE,
            &m_CurrentImageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapchain();
            m_FramePrepared = false;
            return;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        {
            CheckVk(acquireResult, "vkAcquireNextImageKHR");
            m_FramePrepared = false;
            return;
        }

        vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);
        m_FramePrepared = RecordClearCommands(m_CurrentImageIndex);
#endif
    }

    void VulkanRHI::RHIPresent()
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)0;
#else
        if (!m_Initialized || !m_FramePrepared)
        {
            return;
        }

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphores[m_CurrentFrame];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame];

        if (!CheckVk(
                vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]),
                "vkQueueSubmit"))
        {
            m_FramePrepared = false;
            return;
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &m_CurrentImageIndex;

        const VkResult presentResult = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        {
            RecreateSwapchain();
        }
        else if (presentResult != VK_SUCCESS)
        {
            CheckVk(presentResult, "vkQueuePresentKHR");
        }

        m_CurrentFrame = (m_CurrentFrame + 1) % kMaxFramesInFlight;
        m_FramePrepared = false;
#endif
    }

    std::shared_ptr<RHITexture> VulkanRHI::RHICreateTexture2D(
        const RHITextureCreateDesc& desc,
        const void* initialData)
    {
        (void)desc;
        (void)initialData;
        ME_CORE_ERROR("VulkanRHI: RHICreateTexture2D not implemented (S03 clear/present only).");
        return nullptr;
    }

    std::shared_ptr<RHIShaderResourceView> VulkanRHI::RHICreateShaderResourceView(const RHITextureSRVDesc& desc)
    {
        (void)desc;
        ME_CORE_ERROR("VulkanRHI: RHICreateShaderResourceView not implemented (S03).");
        return nullptr;
    }

    std::shared_ptr<RHIBuffer> VulkanRHI::RHICreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData)
    {
        (void)desc;
        (void)initialData;
        ME_CORE_ERROR("VulkanRHI: RHICreateBuffer not implemented (S03).");
        return nullptr;
    }

    std::shared_ptr<RHIShader> VulkanRHI::RHICreateShader(
        const RHIShaderCreateDesc& desc,
        std::string* outCompileLog)
    {
        (void)desc;
        if (outCompileLog)
        {
            *outCompileLog = "VulkanRHI: bytecode shaders land in S04.";
        }
        return nullptr;
    }

    std::shared_ptr<RHIShader> VulkanRHI::RHICreateShader(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outCompileLog)
    {
        (void)vertexSource;
        (void)fragmentSource;
        if (outCompileLog)
        {
            *outCompileLog = "VulkanRHI: GLSL string path unsupported.";
        }
        return nullptr;
    }

    std::shared_ptr<RHIGraphicsPipelineState> VulkanRHI::RHICreateGraphicsPipelineState(
        const RHIGraphicsPSODesc& desc)
    {
        (void)desc;
        return nullptr;
    }

    std::shared_ptr<RHIShaderBindingSetLayout> VulkanRHI::RHICreateShaderBindingSetLayout(
        const std::vector<RHIShaderBindingSetLayoutEntry>& entries)
    {
        (void)entries;
        return nullptr;
    }

    std::shared_ptr<RHIPipelineLayout> VulkanRHI::RHICreatePipelineLayout(
        const std::vector<RHIShaderBindingSetLayout*>& setLayouts)
    {
        (void)setLayouts;
        return nullptr;
    }

    std::shared_ptr<RHIShaderBindingSet> VulkanRHI::RHICreateShaderBindingSet(
        RHIShaderBindingSetLayout* layout,
        const std::vector<RHIShaderBinding>& resources)
    {
        (void)layout;
        (void)resources;
        return nullptr;
    }

    std::shared_ptr<RHIVertexInputLayout> VulkanRHI::RHICreateVertexInputLayout(
        std::initializer_list<RHIVertexElement> elements)
    {
        (void)elements;
        return nullptr;
    }

    void VulkanRHI::RHICmdBeginRenderPass(const RHIRenderPassInfo& info)
    {
        (void)info;
    }

    void VulkanRHI::RHICmdEndRenderPass()
    {
    }

    void VulkanRHI::RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
        (void)pipelineState;
    }

    void VulkanRHI::RHICmdSetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet)
    {
        (void)setIndex;
        (void)bindingSet;
    }

    void VulkanRHI::RHICmdTransition(const RHITextureTransitionInfo& transition)
    {
        (void)transition;
    }

    void VulkanRHI::RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        (void)x;
        (void)y;
        (void)width;
        (void)height;
    }

    void VulkanRHI::RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot)
    {
        (void)vertexBuffer;
        (void)slot;
    }

    void VulkanRHI::RHICmdSetIndexBuffer(RHIBuffer* indexBuffer)
    {
        (void)indexBuffer;
    }

    void VulkanRHI::RHICmdDrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        (void)indexCount;
        (void)firstIndex;
        (void)vertexOffset;
    }

    void VulkanRHI::RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex)
    {
        (void)vertexCount;
        (void)firstVertex;
    }

    void VulkanRHI::RHICmdGenerateMips(RHITexture* texture)
    {
        (void)texture;
    }
}
