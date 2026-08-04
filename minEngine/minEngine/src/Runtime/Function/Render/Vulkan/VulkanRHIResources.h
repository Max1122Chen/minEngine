#pragma once

#include "Runtime/Function/Render/RHI/RHIShader.h"

#include <string>

#if defined(MINENGINE_HAS_VULKAN)
#include <vulkan/vulkan.h>
#endif

namespace minEngine
{
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
}
