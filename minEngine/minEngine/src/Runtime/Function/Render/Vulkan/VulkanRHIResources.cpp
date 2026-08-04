#include "VulkanRHIResources.h"

#include "Runtime/Core/Log/LogSystem.h"

namespace minEngine
{
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
}
