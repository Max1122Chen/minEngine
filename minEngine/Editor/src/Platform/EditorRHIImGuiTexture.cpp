#include "Platform/EditorRHIImGuiTexture.h"

#include "imgui/backends/imgui_impl_vulkan.h"

#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Vulkan/VulkanRHI.h"
#include "Runtime/Function/Render/Vulkan/VulkanRHIResources.h"

#include <vector>

namespace minEngine
{
    namespace
    {
        std::vector<EditorRHIImGuiTexturePin*>& GetTrackedViewportPins()
        {
            static std::vector<EditorRHIImGuiTexturePin*> s_TrackedPins;
            return s_TrackedPins;
        }
    }
    EditorRHIImGuiTexture::~EditorRHIImGuiTexture()
    {
        m_TextureId = ImTextureID_Invalid;
    }

    ImTextureID EditorRHIImGuiTexture::Register(RHI* rhi, RHITexture* texture)
    {
        Release(rhi);

        if (rhi == nullptr || texture == nullptr)
        {
            return ImTextureID_Invalid;
        }

        if (RHIBackendSelection::IsOpenGL())
        {
            m_TextureId = reinterpret_cast<ImTextureID>(
                static_cast<uintptr_t>(GetRHINativeTextureHandle(texture)));
            return m_TextureId;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (!RHIBackendSelection::IsVulkan())
        {
            return ImTextureID_Invalid;
        }

        auto* vulkanTexture = dynamic_cast<VulkanRHITexture*>(texture);
        auto* vulkanRhi = dynamic_cast<VulkanRHI*>(rhi);
        if (vulkanTexture == nullptr || vulkanRhi == nullptr || !vulkanTexture->IsValid())
        {
            return ImTextureID_Invalid;
        }

        VulkanRHI::VulkanEditorFrameInfo frameInfo{};
        vulkanRhi->FillEditorFrameInfo(frameInfo);
        if (frameInfo.DefaultSampler == VK_NULL_HANDLE || vulkanTexture->GetImageView() == VK_NULL_HANDLE)
        {
            return ImTextureID_Invalid;
        }

        const VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(
            frameInfo.DefaultSampler,
            vulkanTexture->GetImageView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (descriptorSet == VK_NULL_HANDLE)
        {
            return ImTextureID_Invalid;
        }

        m_TextureId = reinterpret_cast<ImTextureID>(descriptorSet);
        return m_TextureId;
#else
        (void)rhi;
        (void)texture;
        return ImTextureID_Invalid;
#endif
    }

    void EditorRHIImGuiTexture::Release(RHI* rhi)
    {
        if (m_TextureId == ImTextureID_Invalid)
        {
            return;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (RHIBackendSelection::IsVulkan())
        {
            ReleaseVulkan(rhi);
            return;
        }
#endif

        m_TextureId = ImTextureID_Invalid;
    }

#if defined(MINENGINE_HAS_VULKAN)
    void EditorRHIImGuiTexture::ReleaseVulkan(RHI* rhi)
    {
        (void)rhi;
        if (m_TextureId == ImTextureID_Invalid)
        {
            return;
        }

        ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(m_TextureId));
        m_TextureId = ImTextureID_Invalid;
    }
#endif

    void EditorRHIImGuiTexture::InvalidateAll(RHI* rhi)
    {
        Release(rhi);
    }

    EditorRHIImGuiTexturePin::~EditorRHIImGuiTexturePin()
    {
        UntrackBinding();
        Reset(m_BoundRhi);
    }

    ImTextureID EditorRHIImGuiTexturePin::Pin(RHI* rhi, RHITexture* texture)
    {
        if (texture == nullptr || rhi == nullptr)
        {
            Reset(rhi);
            return ImTextureID_Invalid;
        }

        if (m_Binding.IsValid() && m_BoundRhi == rhi && m_BoundTexture == texture)
        {
            TrackBinding();
            return m_Binding.GetTextureId();
        }

        Reset(rhi);
        m_BoundRhi = rhi;
        m_BoundTexture = texture;
        const ImTextureID textureId = m_Binding.Register(rhi, texture);
        if (textureId != ImTextureID_Invalid)
        {
            TrackBinding();
        }
        return textureId;
    }

    void EditorRHIImGuiTexturePin::Reset(RHI* rhi)
    {
        UntrackBinding();
        m_Binding.Release(rhi != nullptr ? rhi : m_BoundRhi);
        m_BoundRhi = nullptr;
        m_BoundTexture = nullptr;
    }

    void EditorRHIImGuiTexturePin::InvalidateAllBindings()
    {
        InvalidateAllBindingsInternal();
    }

    void EditorRHIImGuiTexturePin::InvalidateAllBindingsInternal()
    {
        std::vector<EditorRHIImGuiTexturePin*> pins = GetTrackedViewportPins();
        for (EditorRHIImGuiTexturePin* pin : pins)
        {
            if (pin != nullptr)
            {
                pin->Reset(pin->m_BoundRhi);
            }
        }
    }

    void EditorRHIImGuiTexturePin::TrackBinding()
    {
        if (m_IsTracked)
        {
            return;
        }

        GetTrackedViewportPins().push_back(this);
        m_IsTracked = true;
    }

    void EditorRHIImGuiTexturePin::UntrackBinding()
    {
        if (!m_IsTracked)
        {
            return;
        }

        std::vector<EditorRHIImGuiTexturePin*>& pins = GetTrackedViewportPins();
        for (auto it = pins.begin(); it != pins.end(); ++it)
        {
            if (*it == this)
            {
                pins.erase(it);
                break;
            }
        }
        m_IsTracked = false;
    }
}
