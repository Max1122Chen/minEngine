#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

namespace minEngine
{
    class ToolbarWindow final : public EditorWindow
    {
    public:
        explicit ToolbarWindow(Editor& editor)
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
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 36.0f));

            ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoTitleBar |
                                            ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoScrollbar |
                                            ImGuiWindowFlags_NoSavedSettings;

            ImGui::Begin(m_Title.c_str(), nullptr, toolbarFlags);
            ImGui::Button(m_Editor.isPlaying ? "Stop" : "Play");

            ImGui::SameLine();
            if (ImGui::Button("Pause"))
            {
            }

            ImGui::SameLine();
            if (ImGui::Button("Step"))
            {
            }

            ImGui::SameLine();
            const float deltaTime = m_Editor.lastDeltaTime;
            ImGui::Text("FPS: %.1f", (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f);
            ImGui::End();
        }

    private:
        const std::string m_Id = "toolbar";
        const std::string m_Title = "Toolbar";
    };
}
