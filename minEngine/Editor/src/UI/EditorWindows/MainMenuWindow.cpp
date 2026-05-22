#include "MainMenuWindow.h"

#include "EditorUIMode.h"

namespace minEngine
{
    void MainMenuWindow::OnDraw()
    {
        const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x, framePadding.y + 3.0f));

        if (!ImGui::BeginMainMenuBar())
        {
            ImGui::PopStyleVar();
            return;
        }

        DrawFileMenu();
        DrawEditMenu();
        DrawViewMenu();
        DrawWindowModeMenu();
        DrawToolsMenu();
        DrawHelpMenu();

        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar();
    }

    void MainMenuWindow::DrawFileMenu()
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            {
            }

            if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
            {
            }

            const bool hasScene = static_cast<bool>(m_Editor.GetActiveScene());
            const bool canSave = hasScene && m_Editor.IsSceneDirty();
            if (ImGui::MenuItem("Save", "Ctrl+S", false, canSave))
            {
                m_Editor.SaveCurrentScene();
            }

            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, false))
            {
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                m_Editor.RequestExit();
            }
            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawEditMenu()
    {
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
    }

    void MainMenuWindow::DrawViewMenu()
    {
        if (ImGui::BeginMenu("View"))
        {
            const EditorUIMode uiMode = m_Editor.GetUIMode();

            if (ImGui::BeginMenu("Panels"))
            {
                for (const auto& window : m_Editor.GetGUIManager().GetWindows())
                {
                    if (window->GetId() == m_Id)
                    {
                        continue;
                    }

                    if (!IsWindowActiveForUIMode(window->GetWindowSuite(), uiMode))
                    {
                        continue;
                    }

                    const bool isOpen = window->IsOpen();
                    if (ImGui::MenuItem(window->GetTitle().c_str(), nullptr, isOpen, true))
                    {
                        window->SetOpen(!isOpen);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Layout"))
            {
                if (ImGui::MenuItem("Reset To Default", nullptr, false, true))
                {
                    m_Editor.requestResetLayout = true;
                    m_Editor.dockLayoutInitialized = false;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawWindowModeMenu()
    {
        if (ImGui::BeginMenu("Window"))
        {
            const bool sceneMode = m_Editor.GetUIMode() == EditorUIMode::SceneEditing;
            const bool materialMode = m_Editor.GetUIMode() == EditorUIMode::MaterialEditing;

            if (ImGui::MenuItem("Scene Editor", nullptr, sceneMode))
            {
                m_Editor.SetUIMode(EditorUIMode::SceneEditing);
            }

            if (ImGui::MenuItem("Material Editor", nullptr, materialMode))
            {
                m_Editor.SetUIMode(EditorUIMode::MaterialEditing);
            }

            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawToolsMenu()
    {
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("Build Settings", nullptr, false, false);
            ImGui::MenuItem("Project Settings", nullptr, false, false);
            ImGui::MenuItem("Profiler", nullptr, false, false);
            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawHelpMenu()
    {
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Documentation", nullptr, false, false);
            ImGui::MenuItem("Shortcuts", nullptr, false, false);
            ImGui::MenuItem("About", nullptr, false, false);
            ImGui::EndMenu();
        }
    }
}
