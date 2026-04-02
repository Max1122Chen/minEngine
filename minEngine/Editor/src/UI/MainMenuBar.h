#pragma once

#include "imgui.h"

#include "Panels/EditorState.h"
#include "Panels/PanelManager.h"

namespace minEngine
{
    class MainMenuBar
    {
    public:
        void Draw(PanelManager& panelManager, EditorState& editorState)
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
                if (ImGui::BeginMenu("Panels"))
                {
                    for (const auto& panel : panelManager.GetPanels())
                    {
                        const bool isOpen = panel->IsOpen();
                        if (ImGui::MenuItem(panel->GetTitle().c_str(), nullptr, isOpen))
                        {
                            panel->SetOpen(!isOpen);
                        }
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Layout"))
                {
                    if (ImGui::MenuItem("Save Layout"))
                    {
                        editorState.requestSaveLayout = true;
                    }

                    if (ImGui::MenuItem("Reset To Default"))
                    {
                        editorState.requestResetLayout = true;
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                ImGui::MenuItem("ImGui Demo", nullptr, &editorState.showDemoWindow);
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
    };
}
