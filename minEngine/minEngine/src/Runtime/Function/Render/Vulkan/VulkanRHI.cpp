#include "VulkanRHI.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"
#include "Runtime/Function/Render/RHI/RHIResourceTransition.h"
#include "Runtime/Function/Render/Vulkan/VulkanRHIResources.h"
#include "Runtime/Function/Render/WindowSystem.h"

#include <algorithm>
#include <cstring>
#include <limits>
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
            !CreateSwapchain() || !CreateCommandResources() || !CreateSwapchainRenderPass() ||
            !CreateSyncObjects() || !CreateDescriptorResources())
        {
            Shutdown();
            ME_CORE_ERROR("VulkanRHI: Initialize failed.");
            return;
        }

        m_Initialized = true;
        ME_CORE_INFO(
            "VulkanRHI Initialized (swapchain {}x{}, format={}, S07b-d descriptors/PSO/cmd)",
            m_SwapchainExtent.width,
            m_SwapchainExtent.height,
            static_cast<int>(m_SwapchainFormat));

        // S07a smoke: create/destroy tiny buffer + Texture2D to catch allocator regressions early.
        {
            RHIBufferCreateDesc probeDesc;
            probeDesc.Usage = RHIBufferUsage::Uniform;
            probeDesc.ByteSize = 64;
            const float probeBytes[16] = {};
            if (RHIBufferRef probe = RHICreateBuffer(probeDesc, probeBytes))
            {
                ME_CORE_INFO("VulkanRHI: S07a buffer probe OK.");
            }
            else
            {
                ME_CORE_ERROR("VulkanRHI: S07a buffer probe failed.");
            }

            RHITextureCreateDesc texDesc;
            texDesc.Dimension = RHITextureDimension::Texture2D;
            texDesc.Width = 4;
            texDesc.Height = 4;
            texDesc.Format = TextureFormat::RGBA8;
            texDesc.Flags = RHITextureCreateFlags::ShaderResource;
            texDesc.NumMips = 1;
            const uint8_t texBytes[4 * 4 * 4] = {};
            if (RHITextureRef tex = RHICreateTexture2D(texDesc, texBytes))
            {
                RHITextureSRVDesc srvDesc;
                srvDesc.Texture = tex.get();
                if (RHICreateShaderResourceView(srvDesc))
                {
                    ME_CORE_INFO("VulkanRHI: S07a texture/SRV probe OK.");
                }
                else
                {
                    ME_CORE_ERROR("VulkanRHI: S07a SRV probe failed.");
                }
            }
            else
            {
                ME_CORE_ERROR("VulkanRHI: S07a texture probe failed.");
            }

            auto layout = RHICreateVertexInputLayout({
                {"a_Position", VertexElementType::Float3},
                {"a_TexCoord", VertexElementType::Float2},
            });
            if (layout && layout->GetStride() == 20)
            {
                ME_CORE_INFO("VulkanRHI: S07a vertex layout probe OK (stride={}).", layout->GetStride());
            }
            else
            {
                ME_CORE_ERROR("VulkanRHI: S07a vertex layout probe failed.");
            }
        }
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

        DestroyDescriptorResources();
        DestroySwapchainRenderPass();
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
        m_FrameRecording = false;
        m_SwapchainDrawnThisFrame = false;
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
        m_SwapchainImageLayouts.assign(swapImageCount, VK_IMAGE_LAYOUT_UNDEFINED);
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
        m_SwapchainImageLayouts.clear();

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }

    void VulkanRHI::RecreateSwapchain()
    {
        vkDeviceWaitIdle(m_Device);
        DestroySwapchainRenderPass();
        DestroySwapchain();
        if (!CreateSwapchain() || !CreateSwapchainRenderPass())
        {
            ME_CORE_ERROR("VulkanRHI: RecreateSwapchain failed.");
            m_FrameRecording = false;
            return;
        }
        m_FrameRecording = false;
        m_SwapchainDrawnThisFrame = false;
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

    bool VulkanRHI::CreateSwapchainRenderPass()
    {
        DestroySwapchainRenderPass();

        if (m_SwapchainImageViews.empty())
        {
            ME_CORE_ERROR("VulkanRHI: swapchain image views empty.");
            return false;
        }

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_SwapchainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // CLEAR + UNDEFINED: discard previous present contents; avoid PRESENT→COLOR transition pitfalls.
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (!CheckVk(
                vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_SwapchainRenderPass),
                "vkCreateRenderPass(swapchain)"))
        {
            return false;
        }

        m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());
        for (size_t i = 0; i < m_SwapchainImageViews.size(); ++i)
        {
            VkImageView attachments[] = {m_SwapchainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_SwapchainRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_SwapchainExtent.width;
            framebufferInfo.height = m_SwapchainExtent.height;
            framebufferInfo.layers = 1;

            if (!CheckVk(
                    vkCreateFramebuffer(
                        m_Device,
                        &framebufferInfo,
                        nullptr,
                        &m_SwapchainFramebuffers[i]),
                    "vkCreateFramebuffer(swapchain)"))
            {
                return false;
            }
        }

        return true;
    }

    void VulkanRHI::DestroySwapchainRenderPass()
    {
        if (m_Device == VK_NULL_HANDLE)
        {
            return;
        }

        for (VkFramebuffer framebuffer : m_SwapchainFramebuffers)
        {
            if (framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
            }
        }
        m_SwapchainFramebuffers.clear();

        if (m_SwapchainRenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_Device, m_SwapchainRenderPass, nullptr);
            m_SwapchainRenderPass = VK_NULL_HANDLE;
        }
    }

    bool VulkanRHI::CreateDescriptorResources()
    {
        DestroyDescriptorResources();

        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 2048;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 2048;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1024;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        if (!CheckVk(
                vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool),
                "vkCreateDescriptorPool"))
        {
            return false;
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        if (!CheckVk(
                vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_DefaultSampler),
                "vkCreateSampler(default)"))
        {
            return false;
        }

        const VulkanDeviceContext context = GetDeviceContext();
        if (!VulkanRHIAllocator::CreateBuffer(
                context,
                256,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                m_DummyUniformBuffer,
                m_DummyUniformMemory))
        {
            ME_CORE_ERROR("VulkanRHI: failed to create dummy uniform buffer.");
            return false;
        }

        if (!VulkanRHIAllocator::CreateImage2D(
                context,
                1,
                1,
                1,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_DummyImage,
                m_DummyImageMemory))
        {
            ME_CORE_ERROR("VulkanRHI: failed to create dummy image.");
            return false;
        }

        if (!VulkanRHIAllocator::CreateImageView2D(
                m_Device,
                m_DummyImage,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_ASPECT_COLOR_BIT,
                1,
                m_DummyImageView))
        {
            ME_CORE_ERROR("VulkanRHI: failed to create dummy image view.");
            return false;
        }

        // Upload opaque white so missing SRVs are visible (not undefined/black).
        const uint8_t whitePixel[4] = {255, 255, 255, 255};
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        if (!VulkanRHIAllocator::CreateBuffer(
                context,
                sizeof(whitePixel),
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer,
                stagingMemory))
        {
            ME_CORE_ERROR("VulkanRHI: failed to create dummy image staging buffer.");
            return false;
        }
        void* mapped = nullptr;
        vkMapMemory(m_Device, stagingMemory, 0, sizeof(whitePixel), 0, &mapped);
        std::memcpy(mapped, whitePixel, sizeof(whitePixel));
        vkUnmapMemory(m_Device, stagingMemory);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd) != VK_SUCCESS)
        {
            VulkanRHIAllocator::DestroyBuffer(m_Device, stagingBuffer, stagingMemory);
            return false;
        }
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        TransitionImage(
            cmd,
            m_DummyImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {1, 1, 1};
        vkCmdCopyBufferToImage(
            cmd,
            stagingBuffer,
            m_DummyImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);
        TransitionImage(
            cmd,
            m_DummyImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);
        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &cmd);
        VulkanRHIAllocator::DestroyBuffer(m_Device, stagingBuffer, stagingMemory);

        return true;
    }

    void VulkanRHI::DestroyDescriptorResources()
    {
        if (m_Device == VK_NULL_HANDLE)
        {
            return;
        }

        for (auto& pair : m_OffscreenFramebuffers)
        {
            if (pair.second != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(m_Device, pair.second, nullptr);
            }
        }
        m_OffscreenFramebuffers.clear();

        for (auto& pair : m_OffscreenRenderPasses)
        {
            if (pair.second != VK_NULL_HANDLE)
            {
                vkDestroyRenderPass(m_Device, pair.second, nullptr);
            }
        }
        m_OffscreenRenderPasses.clear();

        if (m_DummyImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_Device, m_DummyImageView, nullptr);
            m_DummyImageView = VK_NULL_HANDLE;
        }
        VulkanRHIAllocator::DestroyImage(m_Device, m_DummyImage, m_DummyImageMemory);
        VulkanRHIAllocator::DestroyBuffer(m_Device, m_DummyUniformBuffer, m_DummyUniformMemory);

        if (m_DefaultSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(m_Device, m_DefaultSampler, nullptr);
            m_DefaultSampler = VK_NULL_HANDLE;
        }
        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
    }

    VkCommandBuffer VulkanRHI::GetCurrentCommandBuffer() const
    {
        return m_CommandBuffers[m_CurrentFrame];
    }

    bool VulkanRHI::EnsureFrameRecording() const
    {
        return m_Initialized && m_FrameRecording;
    }

    void VulkanRHI::TransitionImage(
        VkCommandBuffer cmd,
        VkImage image,
        VkImageAspectFlags aspect,
        VkImageLayout oldLayout,
        VkImageLayout newLayout)
    {
        if (cmd == VK_NULL_HANDLE || image == VK_NULL_HANDLE || oldLayout == newLayout)
        {
            return;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        auto accessForLayout = [](VkImageLayout layout) -> VkAccessFlags
        {
            switch (layout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                return 0;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return VK_ACCESS_TRANSFER_WRITE_BIT;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return VK_ACCESS_TRANSFER_READ_BIT;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return VK_ACCESS_SHADER_READ_BIT;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                return 0;
            default:
                return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            }
        };

        barrier.srcAccessMask = accessForLayout(oldLayout);
        barrier.dstAccessMask = accessForLayout(newLayout);

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void VulkanRHI::TransitionTextureTo(VulkanRHITexture* texture, VkImageLayout newLayout)
    {
        if (!EnsureFrameRecording() || texture == nullptr || !texture->IsValid())
        {
            return;
        }

        const VkImageLayout oldLayout = texture->GetCurrentLayout();
        if (oldLayout == newLayout)
        {
            return;
        }

        const VkImageAspectFlags aspect = VulkanRHIAllocator::AspectFromFormat(texture->GetDesc().Format);
        TransitionImage(GetCurrentCommandBuffer(), texture->GetImage(), aspect, oldLayout, newLayout);
        texture->SetCurrentLayout(newLayout);
    }

    VkAttachmentLoadOp VulkanRHI::ToVkLoadOp(RHIRenderTargetLoadAction action) const
    {
        switch (action)
        {
        case RHIRenderTargetLoadAction::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case RHIRenderTargetLoadAction::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case RHIRenderTargetLoadAction::NoAction:
        default:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    VkAttachmentStoreOp VulkanRHI::ToVkStoreOp(RHIRenderTargetStoreAction action) const
    {
        switch (action)
        {
        case RHIRenderTargetStoreAction::Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case RHIRenderTargetStoreAction::NoAction:
        default:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    VkRenderPass VulkanRHI::GetOrCreateOffscreenRenderPass(const OffscreenRenderPassKey& key)
    {
        const auto existing = m_OffscreenRenderPasses.find(key);
        if (existing != m_OffscreenRenderPasses.end())
        {
            return existing->second;
        }

        std::vector<VkAttachmentDescription> attachments;
        VkAttachmentReference colorRef{};
        VkAttachmentReference depthRef{};
        int colorIndex = -1;
        int depthIndex = -1;

        if (key.HasColor)
        {
            VkAttachmentDescription color{};
            color.format = key.ColorFormat;
            color.samples = VK_SAMPLE_COUNT_1_BIT;
            color.loadOp = key.ColorLoadOp;
            color.storeOp = key.ColorStoreOp;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorIndex = static_cast<int>(attachments.size());
            attachments.push_back(color);
            colorRef.attachment = static_cast<uint32_t>(colorIndex);
            colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        if (key.HasDepth)
        {
            VkAttachmentDescription depth{};
            depth.format = key.DepthFormat;
            depth.samples = VK_SAMPLE_COUNT_1_BIT;
            depth.loadOp = key.DepthLoadOp;
            depth.storeOp = key.DepthStoreOp;
            depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthIndex = static_cast<int>(attachments.size());
            attachments.push_back(depth);
            depthRef.attachment = static_cast<uint32_t>(depthIndex);
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        if (colorIndex >= 0)
        {
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorRef;
        }
        if (depthIndex >= 0)
        {
            subpass.pDepthStencilAttachment = &depthRef;
        }

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        if (!CheckVk(
                vkCreateRenderPass(m_Device, &createInfo, nullptr, &renderPass),
                "vkCreateRenderPass(offscreen)"))
        {
            return VK_NULL_HANDLE;
        }

        m_OffscreenRenderPasses.emplace(key, renderPass);
        return renderPass;
    }

    VkFramebuffer VulkanRHI::GetOrCreateOffscreenFramebuffer(const OffscreenFramebufferKey& key)
    {
        const auto existing = m_OffscreenFramebuffers.find(key);
        if (existing != m_OffscreenFramebuffers.end())
        {
            return existing->second;
        }

        std::vector<VkImageView> attachments;
        if (key.ColorView != VK_NULL_HANDLE)
        {
            attachments.push_back(key.ColorView);
        }
        if (key.DepthView != VK_NULL_HANDLE)
        {
            attachments.push_back(key.DepthView);
        }

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = key.RenderPass;
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = key.Width;
        createInfo.height = key.Height;
        createInfo.layers = 1;

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        if (!CheckVk(
                vkCreateFramebuffer(m_Device, &createInfo, nullptr, &framebuffer),
                "vkCreateFramebuffer(offscreen)"))
        {
            return VK_NULL_HANDLE;
        }

        m_OffscreenFramebuffers.emplace(key, framebuffer);
        return framebuffer;
    }

    void VulkanRHI::BindPipelineForCurrentRenderPass()
    {
        if (!m_InRenderPass || m_BoundGraphicsPSO == nullptr || m_ActiveRenderPass == VK_NULL_HANDLE)
        {
            return;
        }

        VkPipeline pipeline = m_BoundGraphicsPSO->GetOrCreatePipeline(m_ActiveRenderPass);
        if (pipeline == VK_NULL_HANDLE)
        {
            if (!m_PipelineBindFailureLogged)
            {
                ME_CORE_ERROR(
                    "VulkanRHI: failed to bind graphics pipeline for active render pass "
                    "(draw will be skipped).");
                m_PipelineBindFailureLogged = true;
            }
            return;
        }

        vkCmdBindPipeline(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        if (auto* vertexLayout =
                dynamic_cast<VulkanRHIVertexInputLayout*>(m_BoundGraphicsPSO->GetDesc().VertexInputLayout))
        {
            m_BoundVertexStride = vertexLayout->GetStride();
        }
        else
        {
            m_BoundVertexStride = 0;
        }
    }

    bool VulkanRHI::BeginFrameRecording()
    {
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
            return false;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        {
            CheckVk(acquireResult, "vkAcquireNextImageKHR");
            return false;
        }

        vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

        VkCommandBuffer cmd = GetCurrentCommandBuffer();
        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (!CheckVk(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer"))
        {
            return false;
        }

        m_FrameRecording = true;
        m_SwapchainDrawnThisFrame = false;
        m_InRenderPass = false;
        m_ActiveRenderPass = VK_NULL_HANDLE;
        m_ActiveColorTexture = nullptr;
        m_ActiveDepthTexture = nullptr;
        m_BoundGraphicsPSO = nullptr;
        m_BoundVertexStride = 0;
        m_FrameDrawIndexedCount = 0;
        m_FrameDrawIndexedCalls = 0;
        return true;
    }

    void VulkanRHI::RecordSwapchainClearPass()
    {
        if (!EnsureFrameRecording() || m_SwapchainRenderPass == VK_NULL_HANDLE)
        {
            return;
        }

        VkCommandBuffer cmd = GetCurrentCommandBuffer();
        // Render pass loadOp=CLEAR with initialLayout=UNDEFINED — no pre-transition required.
        (void)m_SwapchainImageLayouts[m_CurrentImageIndex];

        VkClearValue clearValue{};
        clearValue.color.float32[0] = m_ClearColor.x;
        clearValue.color.float32[1] = m_ClearColor.y;
        clearValue.color.float32[2] = m_ClearColor.z;
        clearValue.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = m_SwapchainRenderPass;
        beginInfo.framebuffer = m_SwapchainFramebuffers[m_CurrentImageIndex];
        beginInfo.renderArea.offset = {0, 0};
        beginInfo.renderArea.extent = m_SwapchainExtent;
        beginInfo.clearValueCount = 1;
        beginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(cmd);

        m_SwapchainImageLayouts[m_CurrentImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        m_SwapchainDrawnThisFrame = true;
    }

    void VulkanRHI::EndFrameRecordingAndSubmit()
    {
        if (!m_FrameRecording)
        {
            return;
        }

        if (m_InRenderPass)
        {
            RHICmdEndRenderPass();
        }

        if (m_LoggedDrawIndexedFrameCount < 3 && m_FrameDrawIndexedCalls > 0)
        {
            ME_CORE_INFO(
                "VulkanRHI: frame {} DrawIndexed calls={} totalIndices={}",
                m_LoggedDrawIndexedFrameCount,
                m_FrameDrawIndexedCalls,
                m_FrameDrawIndexedCount);
            ++m_LoggedDrawIndexedFrameCount;
        }

        if (!m_SwapchainDrawnThisFrame)
        {
            RecordSwapchainClearPass();
        }

        VkCommandBuffer cmd = GetCurrentCommandBuffer();
        if (!CheckVk(vkEndCommandBuffer(cmd), "vkEndCommandBuffer"))
        {
            m_FrameRecording = false;
            return;
        }

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphores[m_CurrentFrame];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame];

        if (!CheckVk(
                vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]),
                "vkQueueSubmit"))
        {
            m_FrameRecording = false;
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
        m_FrameRecording = false;
        m_SwapchainDrawnThisFrame = false;
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

        // OUT_OF_DATE recreate returns false; retry once so the frame still records/presents.
        if (!BeginFrameRecording())
        {
            if (!BeginFrameRecording())
            {
                if (!m_BeginFrameFailureLogged)
                {
                    ME_CORE_ERROR(
                        "VulkanRHI: BeginFrameRecording failed twice; frame will not present "
                        "(window may stay black).");
                    m_BeginFrameFailureLogged = true;
                }
                return;
            }
        }
        m_BeginFrameFailureLogged = false;
#endif
    }

    void VulkanRHI::RHIPresent()
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)0;
#else
        if (!m_Initialized)
        {
            return;
        }

        EndFrameRecordingAndSubmit();
#endif
    }

    std::shared_ptr<RHITexture> VulkanRHI::RHICreateTexture2D(
        const RHITextureCreateDesc& desc,
        const void* initialData)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)desc;
        (void)initialData;
        ME_CORE_ERROR("VulkanRHI: built without MINENGINE_HAS_VULKAN.");
        return nullptr;
#else
        if (!m_Initialized)
        {
            ME_CORE_ERROR("VulkanRHI: RHICreateTexture2D before Initialize.");
            return nullptr;
        }

        auto texture = std::make_shared<VulkanRHITexture>(GetDeviceContext(), desc, initialData);
        if (!texture->IsValid())
        {
            return nullptr;
        }
        return texture;
#endif
    }

    std::shared_ptr<RHIShaderResourceView> VulkanRHI::RHICreateShaderResourceView(const RHITextureSRVDesc& desc)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)desc;
        ME_CORE_ERROR("VulkanRHI: built without MINENGINE_HAS_VULKAN.");
        return nullptr;
#else
        if (!m_Initialized)
        {
            ME_CORE_ERROR("VulkanRHI: RHICreateShaderResourceView before Initialize.");
            return nullptr;
        }
        if (desc.Texture == nullptr)
        {
            return nullptr;
        }

        auto srv = std::make_shared<VulkanRHIShaderResourceView>(m_Device, desc);
        if (!srv->IsValid())
        {
            return nullptr;
        }
        return srv;
#endif
    }

    std::shared_ptr<RHIBuffer> VulkanRHI::RHICreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)desc;
        (void)initialData;
        ME_CORE_ERROR("VulkanRHI: built without MINENGINE_HAS_VULKAN.");
        return nullptr;
#else
        if (!m_Initialized)
        {
            ME_CORE_ERROR("VulkanRHI: RHICreateBuffer before Initialize.");
            return nullptr;
        }

        auto buffer = std::make_shared<VulkanRHIBuffer>(GetDeviceContext(), desc, initialData);
        if (!buffer->IsValid())
        {
            return nullptr;
        }
        return buffer;
#endif
    }

    std::shared_ptr<RHIShader> VulkanRHI::RHICreateShader(
        const RHIShaderCreateDesc& desc,
        std::string* outCompileLog)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        if (outCompileLog)
        {
            *outCompileLog = "VulkanRHI: built without MINENGINE_HAS_VULKAN.";
        }
        return nullptr;
#else
        if (m_Device == VK_NULL_HANDLE)
        {
            if (outCompileLog)
            {
                *outCompileLog = "VulkanRHI: device is not initialized.";
            }
            return nullptr;
        }

        auto shader = std::make_shared<VulkanRHIShader>(m_Device, desc);
        if (outCompileLog)
        {
            *outCompileLog = shader->GetCompileLog();
        }
        if (!shader->IsValid())
        {
            return nullptr;
        }
        return shader;
#endif
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
#if !defined(MINENGINE_HAS_VULKAN)
        (void)desc;
        return nullptr;
#else
        if (!m_Initialized || m_Device == VK_NULL_HANDLE)
        {
            return nullptr;
        }
        if (desc.PipelineLayout == nullptr)
        {
            ME_CORE_WARN("RHICreateGraphicsPipelineState: PipelineLayout is null");
        }
        return std::make_shared<VulkanRHIGraphicsPipelineState>(m_Device, desc);
#endif
    }

    std::shared_ptr<RHIShaderBindingSetLayout> VulkanRHI::RHICreateShaderBindingSetLayout(
        const std::vector<RHIShaderBindingSetLayoutEntry>& entries)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)entries;
        return nullptr;
#else
        if (!m_Initialized)
        {
            return nullptr;
        }
        auto layout = std::make_shared<VulkanRHIShaderBindingSetLayout>(m_Device, entries);
        if (!layout->IsValid())
        {
            return nullptr;
        }
        return layout;
#endif
    }

    std::shared_ptr<RHIPipelineLayout> VulkanRHI::RHICreatePipelineLayout(
        const std::vector<RHIShaderBindingSetLayout*>& setLayouts)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)setLayouts;
        return nullptr;
#else
        if (!m_Initialized)
        {
            return nullptr;
        }
        auto layout = std::make_shared<VulkanRHIPipelineLayout>(m_Device, setLayouts);
        if (!layout->IsValid())
        {
            return nullptr;
        }
        return layout;
#endif
    }

    std::shared_ptr<RHIShaderBindingSet> VulkanRHI::RHICreateShaderBindingSet(
        RHIShaderBindingSetLayout* layout,
        const std::vector<RHIShaderBinding>& resources)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)layout;
        (void)resources;
        return nullptr;
#else
        if (!m_Initialized || m_DescriptorPool == VK_NULL_HANDLE)
        {
            return nullptr;
        }
        auto set = std::make_shared<VulkanRHIShaderBindingSet>(
            m_Device,
            m_DescriptorPool,
            m_DefaultSampler,
            m_DummyUniformBuffer,
            m_DummyImageView,
            layout,
            resources);
        if (!set->IsValid())
        {
            return nullptr;
        }
        return set;
#endif
    }

    std::shared_ptr<RHIVertexInputLayout> VulkanRHI::RHICreateVertexInputLayout(
        std::initializer_list<RHIVertexElement> elements)
    {
        return std::make_shared<VulkanRHIVertexInputLayout>(elements);
    }

#if defined(MINENGINE_HAS_VULKAN)
    VulkanDeviceContext VulkanRHI::GetDeviceContext() const
    {
        VulkanDeviceContext context;
        context.Device = m_Device;
        context.PhysicalDevice = m_PhysicalDevice;
        context.GraphicsQueue = m_GraphicsQueue;
        context.CommandPool = m_CommandPool;
        return context;
    }
#endif

    void VulkanRHI::RHICmdBeginRenderPass(const RHIRenderPassInfo& info)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)info;
#else
        if (!EnsureFrameRecording() || m_InRenderPass)
        {
            return;
        }

        VkCommandBuffer cmd = GetCurrentCommandBuffer();
        const RHIRenderPassInfo::ColorAttachment& color0 = info.ColorAttachments[0];
        const bool hasColor = color0.RenderTarget != nullptr;
        const bool hasDepth = info.DepthStencil.DepthStencilTarget != nullptr;

        if (!hasColor && !hasDepth)
        {
            // Swapchain / backbuffer path (PresentPass).
            if (m_SwapchainRenderPass == VK_NULL_HANDLE)
            {
                return;
            }

            // CLEAR+UNDEFINED swapchain RP — skip PRESENT→COLOR transition.
            (void)m_SwapchainImageLayouts[m_CurrentImageIndex];

            VkClearValue clearValue{};
            clearValue.color.float32[0] = m_ClearColor.x;
            clearValue.color.float32[1] = m_ClearColor.y;
            clearValue.color.float32[2] = m_ClearColor.z;
            clearValue.color.float32[3] = 1.0f;

            VkRenderPassBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = m_SwapchainRenderPass;
            beginInfo.framebuffer = m_SwapchainFramebuffers[m_CurrentImageIndex];
            beginInfo.renderArea.offset = {0, 0};
            beginInfo.renderArea.extent = m_SwapchainExtent;
            beginInfo.clearValueCount = 1;
            beginInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
            m_ActiveRenderPass = m_SwapchainRenderPass;
            m_ActiveColorTexture = nullptr;
            m_ActiveDepthTexture = nullptr;
            m_InRenderPass = true;
            m_SwapchainDrawnThisFrame = true;
            BindPipelineForCurrentRenderPass();
            return;
        }

        auto* colorTexture = hasColor ? dynamic_cast<VulkanRHITexture*>(color0.RenderTarget) : nullptr;
        auto* depthTexture =
            hasDepth ? dynamic_cast<VulkanRHITexture*>(info.DepthStencil.DepthStencilTarget) : nullptr;
        if ((hasColor && (colorTexture == nullptr || !colorTexture->IsValid())) ||
            (hasDepth && (depthTexture == nullptr || !depthTexture->IsValid())))
        {
            ME_CORE_ERROR("VulkanRHI: BeginRenderPass attachments are not Vulkan textures.");
            return;
        }

        if (hasColor)
        {
            TransitionTextureTo(colorTexture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
        if (hasDepth)
        {
            TransitionTextureTo(depthTexture, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        OffscreenRenderPassKey passKey{};
        passKey.HasColor = hasColor;
        passKey.HasDepth = hasDepth;
        if (hasColor)
        {
            passKey.ColorFormat = colorTexture->GetVkFormat();
            passKey.ColorLoadOp = ToVkLoadOp(GetLoadAction(color0.Action));
            passKey.ColorStoreOp = ToVkStoreOp(GetStoreAction(color0.Action));
        }
        if (hasDepth)
        {
            passKey.DepthFormat = depthTexture->GetVkFormat();
            const RHIRenderTargetActions depthActions = static_cast<RHIRenderTargetActions>(
                static_cast<uint8_t>(info.DepthStencil.Action) >>
                static_cast<uint8_t>(RHIDepthStencilTargetActions::DepthMask));
            passKey.DepthLoadOp = ToVkLoadOp(GetLoadAction(depthActions));
            passKey.DepthStoreOp = ToVkStoreOp(GetStoreAction(depthActions));
        }

        VkRenderPass renderPass = GetOrCreateOffscreenRenderPass(passKey);
        if (renderPass == VK_NULL_HANDLE)
        {
            return;
        }

        const uint32_t width = hasColor ? colorTexture->GetDesc().Width : depthTexture->GetDesc().Width;
        const uint32_t height = hasColor ? colorTexture->GetDesc().Height : depthTexture->GetDesc().Height;

        OffscreenFramebufferKey fbKey{};
        fbKey.RenderPass = renderPass;
        fbKey.ColorView = hasColor ? colorTexture->GetImageView() : VK_NULL_HANDLE;
        fbKey.DepthView = hasDepth ? depthTexture->GetImageView() : VK_NULL_HANDLE;
        fbKey.Width = width;
        fbKey.Height = height;
        VkFramebuffer framebuffer = GetOrCreateOffscreenFramebuffer(fbKey);
        if (framebuffer == VK_NULL_HANDLE)
        {
            return;
        }

        std::array<VkClearValue, 2> clearValues{};
        uint32_t clearCount = 0;
        if (hasColor)
        {
            clearValues[clearCount].color.float32[0] = info.ClearValue.Color[0];
            clearValues[clearCount].color.float32[1] = info.ClearValue.Color[1];
            clearValues[clearCount].color.float32[2] = info.ClearValue.Color[2];
            clearValues[clearCount].color.float32[3] = info.ClearValue.Color[3];
            ++clearCount;
        }
        if (hasDepth)
        {
            clearValues[clearCount].depthStencil.depth = info.ClearValue.Depth;
            clearValues[clearCount].depthStencil.stencil = info.ClearValue.Stencil;
            ++clearCount;
        }

        VkRenderPassBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = renderPass;
        beginInfo.framebuffer = framebuffer;
        beginInfo.renderArea.offset = {0, 0};
        beginInfo.renderArea.extent = {width, height};
        beginInfo.clearValueCount = clearCount;
        beginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        m_ActiveRenderPass = renderPass;
        m_ActiveColorTexture = colorTexture;
        m_ActiveDepthTexture = depthTexture;
        m_InRenderPass = true;
        BindPipelineForCurrentRenderPass();
#endif
    }

    void VulkanRHI::RHICmdEndRenderPass()
    {
#if defined(MINENGINE_HAS_VULKAN)
        if (!EnsureFrameRecording() || !m_InRenderPass)
        {
            return;
        }

        vkCmdEndRenderPass(GetCurrentCommandBuffer());

        if (m_ActiveColorTexture == nullptr && m_ActiveDepthTexture == nullptr)
        {
            m_SwapchainImageLayouts[m_CurrentImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
        else
        {
            if (m_ActiveColorTexture != nullptr)
            {
                m_ActiveColorTexture->SetCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                if (HasTextureCreateFlag(
                        m_ActiveColorTexture->GetDesc().Flags,
                        RHITextureCreateFlags::ShaderResource))
                {
                    TransitionTextureTo(m_ActiveColorTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
            }
            if (m_ActiveDepthTexture != nullptr)
            {
                m_ActiveDepthTexture->SetCurrentLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                if (HasTextureCreateFlag(
                        m_ActiveDepthTexture->GetDesc().Flags,
                        RHITextureCreateFlags::ShaderResource))
                {
                    TransitionTextureTo(m_ActiveDepthTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
            }
        }

        m_InRenderPass = false;
        m_ActiveRenderPass = VK_NULL_HANDLE;
        m_ActiveColorTexture = nullptr;
        m_ActiveDepthTexture = nullptr;
#endif
    }

    void VulkanRHI::RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)pipelineState;
#else
        if (!EnsureFrameRecording())
        {
            return;
        }

        m_BoundGraphicsPSO = dynamic_cast<VulkanRHIGraphicsPipelineState*>(pipelineState);
        BindPipelineForCurrentRenderPass();
#endif
    }

    void VulkanRHI::RHICmdSetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)setIndex;
        (void)bindingSet;
#else
        if (!EnsureFrameRecording() || bindingSet == nullptr || m_BoundGraphicsPSO == nullptr)
        {
            return;
        }

        auto* vulkanSet = dynamic_cast<VulkanRHIShaderBindingSet*>(bindingSet);
        VkPipelineLayout pipelineLayout = m_BoundGraphicsPSO->GetVkPipelineLayout();
        if (vulkanSet == nullptr || !vulkanSet->IsValid() || pipelineLayout == VK_NULL_HANDLE)
        {
            return;
        }

        VkDescriptorSet descriptorSet = vulkanSet->GetVkDescriptorSet();
        vkCmdBindDescriptorSets(
            GetCurrentCommandBuffer(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            setIndex,
            1,
            &descriptorSet,
            0,
            nullptr);
#endif
    }

    void VulkanRHI::RHICmdTransition(const RHITextureTransitionInfo& transition)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)transition;
#else
        if (!EnsureFrameRecording() || transition.Texture == nullptr)
        {
            return;
        }

        auto* texture = dynamic_cast<VulkanRHITexture*>(transition.Texture);
        if (texture == nullptr || !texture->IsValid())
        {
            return;
        }

        // Public transition is intent-only today; move sampled textures to shader-read.
        TransitionTextureTo(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
    }

    void VulkanRHI::RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)x;
        (void)y;
        (void)width;
        (void)height;
#else
        if (!EnsureFrameRecording())
        {
            return;
        }

        VkViewport viewport{};
        viewport.x = static_cast<float>(x);
        // GLM/OpenGL-style projection: flip viewport Y (Vulkan NDC Y points down).
        viewport.y = static_cast<float>(y + height);
        viewport.width = static_cast<float>(width);
        viewport.height = -static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
        scissor.extent = {width, height};

        VkCommandBuffer cmd = GetCurrentCommandBuffer();
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
#endif
    }

    void VulkanRHI::RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)vertexBuffer;
        (void)slot;
#else
        if (!EnsureFrameRecording() || vertexBuffer == nullptr)
        {
            return;
        }

        auto* buffer = dynamic_cast<VulkanRHIBuffer*>(vertexBuffer);
        if (buffer == nullptr || !buffer->IsValid())
        {
            return;
        }

        VkBuffer vkBuffer = buffer->GetBuffer();
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(GetCurrentCommandBuffer(), slot, 1, &vkBuffer, &offset);
        (void)m_BoundVertexStride;
#endif
    }

    void VulkanRHI::RHICmdSetIndexBuffer(RHIBuffer* indexBuffer)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)indexBuffer;
#else
        if (!EnsureFrameRecording() || indexBuffer == nullptr)
        {
            return;
        }

        auto* buffer = dynamic_cast<VulkanRHIBuffer*>(indexBuffer);
        if (buffer == nullptr || !buffer->IsValid())
        {
            return;
        }

        vkCmdBindIndexBuffer(GetCurrentCommandBuffer(), buffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
#endif
    }

    void VulkanRHI::RHICmdDrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)indexCount;
        (void)firstIndex;
        (void)vertexOffset;
#else
        if (!EnsureFrameRecording() || !m_InRenderPass)
        {
            return;
        }

        m_FrameDrawIndexedCount += indexCount;
        ++m_FrameDrawIndexedCalls;
        if (!m_DrawIndexedLogged)
        {
            ME_CORE_INFO(
                "VulkanRHI: first DrawIndexed count={} firstIndex={} vertexOffset={}",
                indexCount,
                firstIndex,
                vertexOffset);
            m_DrawIndexedLogged = true;
        }

        vkCmdDrawIndexed(
            GetCurrentCommandBuffer(),
            indexCount,
            1,
            firstIndex,
            vertexOffset,
            0);
#endif
    }

    void VulkanRHI::RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex)
    {
#if !defined(MINENGINE_HAS_VULKAN)
        (void)vertexCount;
        (void)firstVertex;
#else
        if (!EnsureFrameRecording() || !m_InRenderPass)
        {
            return;
        }

        vkCmdDraw(GetCurrentCommandBuffer(), vertexCount, 1, firstVertex, 0);
#endif
    }

    void VulkanRHI::RHICmdGenerateMips(RHITexture* texture)
    {
        (void)texture;
#if defined(MINENGINE_HAS_VULKAN)
        if (!m_GenerateMipsWarned)
        {
            ME_CORE_WARN("VulkanRHI: RHICmdGenerateMips is a no-op in S07d.");
            m_GenerateMipsWarned = true;
        }
#endif
    }
}
