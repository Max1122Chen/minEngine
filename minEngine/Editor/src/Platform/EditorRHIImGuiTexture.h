#pragma once

#include "imgui.h"

namespace minEngine
{
    class RHI;
    class RHITexture;

    /** ED-F01: RHITexture to ImGui::Image ImTextureID (GL handle or VkDescriptorSet). */
    class EditorRHIImGuiTexture
    {
    public:
        EditorRHIImGuiTexture() = default;
        ~EditorRHIImGuiTexture();

        EditorRHIImGuiTexture(const EditorRHIImGuiTexture&) = delete;
        EditorRHIImGuiTexture& operator=(const EditorRHIImGuiTexture&) = delete;

        ImTextureID Register(RHI* rhi, RHITexture* texture);
        void Release(RHI* rhi);

        void InvalidateAll(RHI* rhi);

        ImTextureID GetTextureId() const { return m_TextureId; }
        bool IsValid() const { return m_TextureId != ImTextureID_Invalid; }

    private:
        ImTextureID m_TextureId = ImTextureID_Invalid;
#if defined(MINENGINE_HAS_VULKAN)
        void ReleaseVulkan(RHI* rhi);
#endif
    };

    /** Per-frame pin: keep ImGui texture registration alive while Image() is drawn. */
    class EditorRHIImGuiTexturePin
    {
    public:
        EditorRHIImGuiTexturePin() = default;
        ~EditorRHIImGuiTexturePin();

        EditorRHIImGuiTexturePin(const EditorRHIImGuiTexturePin&) = delete;
        EditorRHIImGuiTexturePin& operator=(const EditorRHIImGuiTexturePin&) = delete;

        ImTextureID Pin(RHI* rhi, RHITexture* texture);
        void Reset(RHI* rhi);

    private:
        EditorRHIImGuiTexture m_Binding;
        RHI* m_BoundRhi = nullptr;
        RHITexture* m_BoundTexture = nullptr;
    };
}
