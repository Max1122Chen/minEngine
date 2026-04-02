#pragma once

#include <string>

#include "imgui.h"

#include "IPanel.h"

namespace minEngine
{
    class ToolbarPanel final : public IPanel
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

        void OnDraw(const PanelContext& context) override
        {
            if (context.state == nullptr)
            {
                return;
            }

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 36.0f));

            ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoTitleBar |
                                            ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoScrollbar |
                                            ImGuiWindowFlags_NoSavedSettings;

            ImGui::Begin(m_Title.c_str(), nullptr, toolbarFlags);
            if (ImGui::Button(context.state->isPlaying ? "Stop" : "Play"))
            {
                context.state->isPlaying = !context.state->isPlaying;
            }

            ImGui::SameLine();
            if (ImGui::Button("Pause"))
            {
            }

            ImGui::SameLine();
            if (ImGui::Button("Step"))
            {
            }

            ImGui::SameLine();
            ImGui::Checkbox("Demo", &context.state->showDemoWindow);

            ImGui::SameLine();
            ImGui::Text("FPS: %.1f", (context.state->lastDeltaTime > 0.0f) ? (1.0f / context.state->lastDeltaTime) : 0.0f);
            ImGui::End();
        }

    private:
        const std::string m_Id = "toolbar";
        const std::string m_Title = "Toolbar";
    };
}
