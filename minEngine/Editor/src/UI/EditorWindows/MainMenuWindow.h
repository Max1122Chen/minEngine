#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

namespace minEngine
{
    class MainMenuWindow final : public EditorWindow
    {
    public:
        explicit MainMenuWindow(Editor& editor)
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
            const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x, framePadding.y + 3.0f));

            if (!ImGui::BeginMainMenuBar())
            {
                ImGui::PopStyleVar();
                return;
            }

            if (ImGui::BeginMenu("File"))
            {
                ImGui::MenuItem("New Scene", "Ctrl+N", false, false);
                ImGui::MenuItem("Open Scene...", "Ctrl+O", false, false);
                ImGui::MenuItem("Save", "Ctrl+S", false, false);
                ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, false);
                ImGui::Separator();
                ImGui::MenuItem("Exit", nullptr, false, false);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
                ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
                ImGui::Separator();
                ImGui::MenuItem("Cut", "Ctrl+X", false, false);
                ImGui::MenuItem("Copy", "Ctrl+C", false, false);
                ImGui::MenuItem("Paste", "Ctrl+V", false, false);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::BeginMenu("Windows"))
                {
                    for (const auto& window : m_Editor.GetGUIManager().GetWindows())
                    {
                        if (window->GetId() == m_Id)
                        {
                            continue;
                        }

                        const bool isOpen = window->IsOpen();
                        ImGui::MenuItem(window->GetTitle().c_str(), nullptr, isOpen, false);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Layout"))
                {
                    ImGui::MenuItem("Reset To Default", nullptr, false, false);
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                ImGui::MenuItem("ImGui Demo", nullptr, m_Editor.showDemoWindow, false);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools"))
            {
                ImGui::MenuItem("Build Settings", nullptr, false, false);
                ImGui::MenuItem("Project Settings", nullptr, false, false);
                ImGui::MenuItem("Profiler", nullptr, false, false);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                ImGui::MenuItem("Documentation", nullptr, false, false);
                ImGui::MenuItem("Shortcuts", nullptr, false, false);
                ImGui::MenuItem("About", nullptr, false, false);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
            ImGui::PopStyleVar();
        }

    private:
        const std::string m_Id = "main_menu";
        const std::string m_Title = "MainMenu";
    };
}
