#pragma once

#include <cstdint>
#include <cmath>
#include <string>

#include "imgui.h"

#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/RuntimeGlobalContext.h"

#include "IPanel.h"

namespace minEngine
{
    class ViewportPanel final : public IPanel
    {
    public:
        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw(const PanelContext&) override
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 280.0f, viewport->Pos.y + 44.0f), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - 560.0f, viewport->Size.y - 280.0f), ImGuiCond_Once);

            ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoScrollbar |
                                             ImGuiWindowFlags_NoScrollWithMouse |
                                             ImGuiWindowFlags_NoCollapse;

            ImGui::Begin(m_Title.c_str(), nullptr, viewportFlags);
            ImVec2 avail = ImGui::GetContentRegionAvail();

            const auto& renderSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem;
            if (!renderSystem)
            {
                ImGui::TextWrapped("RenderSystem is not ready.");
                ImGui::End();
                return;
            }

            const uint32_t viewportWidth = static_cast<uint32_t>(std::max(1.0f, std::round(avail.x)));
            const uint32_t viewportHeight = static_cast<uint32_t>(std::max(1.0f, std::round(avail.y)));
            if (viewportWidth != m_LastRequestedWidth || viewportHeight != m_LastRequestedHeight)
            {
                renderSystem->RequestSceneViewportResize(viewportWidth, viewportHeight);
                m_LastRequestedWidth = viewportWidth;
                m_LastRequestedHeight = viewportHeight;
            }

            const std::shared_ptr<RHITexture2D>& sceneColor = renderSystem->GetSceneColorTexture();
            // std::shared_ptr<Texture2D> sceneColor = RuntimeGlobalContext::GetRuntimeGlobalContext().m_AssetManager->LoadTexture2D("D:/Dev/GitRepo/minEngine/minEngine/Assets/Textures/window.png", 0);
            if (!sceneColor)
            {
                ImGui::TextWrapped("Scene color texture is not ready.");
                ImGui::End();
                return;
            }

            // const ImTextureID textureID = static_cast<ImTextureID>(sceneColor->GetRHITexture()->GetID());
            const ImTextureID textureID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(sceneColor->GetID()));
            ImGui::Image(textureID, avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::End();
        }

    private:
        const std::string m_Id = "viewport";
        const std::string m_Title = "Viewport";
        uint32_t m_LastRequestedWidth = 0;
        uint32_t m_LastRequestedHeight = 0;
    };
}
