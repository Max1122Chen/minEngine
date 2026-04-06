#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"

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
                if (ImGui::MenuItem("New Scene", "Ctrl+N"))
                {
                    QueueFileAction(PendingFileAction::NewScene, "Assets/Scenes/EditorDefault.scene.json");
                }

                if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
                {
                    QueueFileAction(PendingFileAction::OpenScene, "Assets/Scenes/EditorDefault.scene.json");
                }

                const bool hasScene = static_cast<bool>(m_Editor.GetActiveScene());
                const bool canSave = hasScene && m_Editor.IsSceneDirty();
                if (ImGui::MenuItem("Save", "Ctrl+S", false, canSave))
                {
                    m_Editor.SaveCurrentScene();
                }

                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, hasScene))
                {
                    std::filesystem::path sourcePath = m_Editor.GetCurrentScenePath();
                    const std::string stem = sourcePath.stem().string().empty() ? std::string("Scene") : sourcePath.stem().string();
                    std::filesystem::path saveAsPath = sourcePath.parent_path() / (stem + "_SaveAs.scene.json");
                    if (saveAsPath.empty())
                    {
                        saveAsPath = std::filesystem::path("Assets/Scenes/EditorDefault_SaveAs.scene.json");
                    }
                    m_Editor.SaveCurrentSceneAs(saveAsPath);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
                {
                    QueueFileAction(PendingFileAction::ExitEditor, "");
                }
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

            DrawUnsavedChangesPopup();
        }

    private:
        enum class PendingFileAction
        {
            None,
            NewScene,
            OpenScene,
            ExitEditor,
        };

        void QueueFileAction(PendingFileAction action, std::string targetPath)
        {
            if (!m_Editor.IsSceneDirty())
            {
                ExecuteFileAction(action, targetPath);
                return;
            }

            m_PendingAction = action;
            m_PendingPath = std::move(targetPath);
            ImGui::OpenPopup("Unsaved Scene Changes");
        }

        void ExecutePendingAction()
        {
            ExecuteFileAction(m_PendingAction, m_PendingPath);
            m_PendingAction = PendingFileAction::None;
            m_PendingPath.clear();
        }

        void ExecuteFileAction(PendingFileAction action, const std::string& targetPath)
        {
            switch (action)
            {
            case PendingFileAction::NewScene:
                m_Editor.CreateNewScene(targetPath);
                break;
            case PendingFileAction::OpenScene:
                m_Editor.OpenScene(targetPath);
                break;
            case PendingFileAction::ExitEditor:
                m_Editor.RequestExit();
                break;
            case PendingFileAction::None:
            default:
                break;
            }
        }

        void DrawUnsavedChangesPopup()
        {
            if (ImGui::BeginPopupModal("Unsaved Scene Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted("Current scene has unsaved changes.");
                ImGui::TextUnformatted("Do you want to save before continuing?");
                ImGui::Spacing();

                if (ImGui::Button("Save", ImVec2(110.0f, 0.0f)))
                {
                    if (m_Editor.SaveCurrentScene())
                    {
                        ExecutePendingAction();
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Don't Save", ImVec2(110.0f, 0.0f)))
                {
                    ExecutePendingAction();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f)))
                {
                    m_PendingAction = PendingFileAction::None;
                    m_PendingPath.clear();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        const std::string m_Id = "main_menu";
        const std::string m_Title = "MainMenu";
        PendingFileAction m_PendingAction = PendingFileAction::None;
        std::string m_PendingPath;
    };
}
