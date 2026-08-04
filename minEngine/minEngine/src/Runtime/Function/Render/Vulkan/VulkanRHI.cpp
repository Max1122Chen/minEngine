#include "VulkanRHI.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Render/ShaderCompiler/ShaderCompiler.h"
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
            !CreateSwapchain() || !CreateCommandResources() || !CreateRenderPassAndGraphicsPipeline() ||
            !CreateSyncObjects())
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

        DestroyGraphicsPipelineAndRenderPass();

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
        DestroyGraphicsPipelineAndRenderPass();
        DestroySwapchain();
        if (!CreateSwapchain() || !CreateRenderPassAndGraphicsPipeline())
        {
            ME_CORE_ERROR("VulkanRHI: RecreateSwapchain failed.");
            m_FramePrepared = false;
            return;
        }
        m_FramePrepared = false;
        m_HasRenderedOnce = false;
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

    bool VulkanRHI::CreateRenderPassAndGraphicsPipeline()
    {
        DestroyGraphicsPipelineAndRenderPass();

        if (m_SwapchainImageViews.empty())
        {
            ME_CORE_ERROR("VulkanRHI: swapchain image views empty.");
            return false;
        }

        const std::string vertexGlsl = R"glsl(
#version 450
layout(location=0) out vec3 v_Color;
void main()
{
    vec2 pos[3] = vec2[3](
        vec2( 0.0, -0.65),
        vec2( 0.65,  0.55),
        vec2(-0.65,  0.55)
    );
    vec3 colors[3] = vec3[3](
        vec3(1.0, 0.2, 0.2),
        vec3(0.2, 1.0, 0.2),
        vec3(0.2, 0.4, 1.0)
    );
    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
    v_Color = colors[gl_VertexIndex];
}
)glsl";

        const std::string fragmentGlsl = R"glsl(
#version 450
layout(location=0) in vec3 v_Color;
layout(location=0) out vec4 FragColor;
void main()
{
    FragColor = vec4(v_Color, 1.0);
}
)glsl";

        ShaderCompiler& shaderCompiler = ShaderCompiler::Get();

        ShaderCompileRequest vsReq{};
        vsReq.Source = vertexGlsl;
        vsReq.Stage = ShaderCompilerStage::Vertex;
        vsReq.Target = ShaderSpirvTarget::Vulkan;
        vsReq.EntryPoint = "main";
        vsReq.DebugName = "VulkanTriangle.vert";

        ShaderCompileResult vsRes = shaderCompiler.Compile(vsReq);
        if (!vsRes.Success)
        {
            ME_CORE_ERROR("VulkanRHI: failed to compile vertex shader: {}", vsRes.Log);
            return false;
        }

        ShaderCompileRequest fsReq{};
        fsReq.Source = fragmentGlsl;
        fsReq.Stage = ShaderCompilerStage::Fragment;
        fsReq.Target = ShaderSpirvTarget::Vulkan;
        fsReq.EntryPoint = "main";
        fsReq.DebugName = "VulkanTriangle.frag";

        ShaderCompileResult fsRes = shaderCompiler.Compile(fsReq);
        if (!fsRes.Success)
        {
            ME_CORE_ERROR("VulkanRHI: failed to compile fragment shader: {}", fsRes.Log);
            return false;
        }

        VkShaderModuleCreateInfo shaderModuleInfo{};
        shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        shaderModuleInfo.codeSize = vsRes.SpirvWords.size() * sizeof(uint32_t);
        shaderModuleInfo.pCode = vsRes.SpirvWords.data();
        if (!CheckVk(
                vkCreateShaderModule(m_Device, &shaderModuleInfo, nullptr, &m_VertShaderModule),
                "vkCreateShaderModule(vert)"))
        {
            return false;
        }

        shaderModuleInfo.codeSize = fsRes.SpirvWords.size() * sizeof(uint32_t);
        shaderModuleInfo.pCode = fsRes.SpirvWords.data();
        if (!CheckVk(
                vkCreateShaderModule(m_Device, &shaderModuleInfo, nullptr, &m_FragShaderModule),
                "vkCreateShaderModule(frag)"))
        {
            return false;
        }

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        if (!CheckVk(vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_PipelineLayout), "vkCreatePipelineLayout"))
        {
            return false;
        }

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_SwapchainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

        if (!CheckVk(vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass), "vkCreateRenderPass"))
        {
            return false;
        }

        m_Framebuffers.resize(m_SwapchainImageViews.size());
        for (size_t i = 0; i < m_SwapchainImageViews.size(); ++i)
        {
            VkImageView attachments[] = {m_SwapchainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_SwapchainExtent.width;
            framebufferInfo.height = m_SwapchainExtent.height;
            framebufferInfo.layers = 1;

            if (!CheckVk(vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_Framebuffers[i]), "vkCreateFramebuffer"))
            {
                return false;
            }
        }

        VkPipelineShaderStageCreateInfo vertStageInfo{};
        vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = m_VertShaderModule;
        vertStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragStageInfo{};
        fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = m_FragShaderModule;
        fragStageInfo.pName = "main";

        const VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_SwapchainExtent.width);
        viewport.height = static_cast<float>(m_SwapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_SwapchainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = m_RenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (!CheckVk(
                vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline),
                "vkCreateGraphicsPipelines"))
        {
            return false;
        }

        return true;
    }

    void VulkanRHI::DestroyGraphicsPipelineAndRenderPass()
    {
#if defined(MINENGINE_HAS_VULKAN)
        if (m_Device == VK_NULL_HANDLE)
        {
            return;
        }

        for (VkFramebuffer framebuffer : m_Framebuffers)
        {
            if (framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
            }
        }
        m_Framebuffers.clear();

        if (m_GraphicsPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
            m_GraphicsPipeline = VK_NULL_HANDLE;
        }

        if (m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
            m_PipelineLayout = VK_NULL_HANDLE;
        }

        if (m_VertShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_Device, m_VertShaderModule, nullptr);
            m_VertShaderModule = VK_NULL_HANDLE;
        }

        if (m_FragShaderModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_Device, m_FragShaderModule, nullptr);
            m_FragShaderModule = VK_NULL_HANDLE;
        }

        if (m_RenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
            m_RenderPass = VK_NULL_HANDLE;
        }

        m_HasRenderedOnce = false;
#endif
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

        VkImageMemoryBarrier toColorAttachment{};
        toColorAttachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toColorAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColorAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColorAttachment.image = m_SwapchainImages[imageIndex];
        toColorAttachment.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toColorAttachment.subresourceRange.baseMipLevel = 0;
        toColorAttachment.subresourceRange.levelCount = 1;
        toColorAttachment.subresourceRange.baseArrayLayer = 0;
        toColorAttachment.subresourceRange.layerCount = 1;
        toColorAttachment.oldLayout = m_HasRenderedOnce ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
        toColorAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColorAttachment.srcAccessMask = 0;
        toColorAttachment.dstAccessMask = 0;

        vkCmdPipelineBarrier(
            cmd,
            m_HasRenderedOnce ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toColorAttachment);

        VkClearValue clearValue{};
        clearValue.color.float32[0] = m_ClearColor.x;
        clearValue.color.float32[1] = m_ClearColor.y;
        clearValue.color.float32[2] = m_ClearColor.z;
        clearValue.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo renderPassBegin{};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = m_RenderPass;
        renderPassBegin.framebuffer = m_Framebuffers[imageIndex];
        renderPassBegin.renderArea.offset = {0, 0};
        renderPassBegin.renderArea.extent = m_SwapchainExtent;
        renderPassBegin.clearValueCount = 1;
        renderPassBegin.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmd, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);

        m_HasRenderedOnce = true;

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
