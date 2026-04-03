#pragma once
#include "Core.h"

#include "imgui.h"

#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"

#include "EditorWindow.h"

namespace minEngine
{
    class ViewportWindow final : public EditorWindow
    {
    public:
        explicit ViewportWindow(Editor& editor)
            : EditorWindow(editor)
        {
        }

        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw() override
        {
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

            const std::shared_ptr<RHITexture2D>& sceneColor = renderSystem->GetSceneColorTexture();
            if (!sceneColor)
            {
                ImGui::TextWrapped("Scene color texture is not ready.");
                ImGui::End();
                return;
            }

            const ImTextureID textureID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(sceneColor->GetID()));
            ImGui::Image(textureID, avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::End();
        }

    private:
        const std::string m_Id = "viewport";
        const std::string m_Title = "Viewport";
    };
}
