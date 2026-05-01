#include "HierarchyWindow.h"

namespace minEngine
{
    void HierarchyWindow::OnDraw()
    {
        ImGui::Begin(m_Title.c_str());

        if (ImGui::Button("Create Empty"))
        {
            m_Editor.AddEmptyGOToScene();
        }
        ImGui::Separator();

        const std::vector<GameObject*> gameObjects = m_Editor.GetHierarchyGameObjects();
        if (gameObjects.empty())
        {
            ImGui::TextUnformatted("No GameObject in current scene.");
            ImGui::End();
            return;
        }

        TryCaptureF2RenameRequest();

        // For each GO in hierarchy, we draw a selectable item. Clicking on it will select the GO, and right-clicking will open a context menu for that GO.
        bool anyGoMenuOpened = false;
        for (GameObject* gameObject : gameObjects)
        {
            if (!gameObject)
            {
                continue;
            }

            ImGui::PushID(static_cast<int>(gameObject->GetID()));

            if (m_RenamingGameObjectId == gameObject->GetID())
            {
                if (m_RequestRenameFocus)
                {
                    ImGui::SetKeyboardFocusHere();
                    m_RequestRenameFocus = false;
                }

                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.19f, 0.28f, 0.40f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.24f, 0.35f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.32f, 0.46f, 1.0f));
                const bool committed = ImGui::InputText("##Rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                                                        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopStyleColor(3);

                if (committed || ImGui::IsItemDeactivatedAfterEdit())
                {
                    m_Editor.RenameGameObject(gameObject->GetID(), m_RenameBuffer);
                    m_RenamingGameObjectId = kInvalidGameObjectId;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                {
                    m_RenamingGameObjectId = kInvalidGameObjectId;
                }

                ImGui::PopID();
                continue;
            }

            const bool selected = m_Editor.IsGameObjectSelected(gameObject->GetID());
            const std::string displayName = m_Editor.GetGameObjectDisplayName(*gameObject);
            const std::string label = std::string("  ") + displayName;

            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.23f, 0.36f, 0.54f, 0.75f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.27f, 0.42f, 0.61f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.21f, 0.33f, 0.49f, 0.95f));
            }

            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                m_Editor.SelectGameObject(gameObject->GetID());
            }

            // Custom selection highlight
            if (selected)
            {
                ImGui::PopStyleColor(3);
                ImVec2 minPos = ImGui::GetItemRectMin();
                ImVec2 maxPos = ImGui::GetItemRectMax();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(ImVec2(minPos.x + 3.0f, minPos.y + 4.0f), ImVec2(minPos.x + 7.0f, maxPos.y - 4.0f), IM_COL32(102, 178, 255, 255), 2.0f);
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_Editor.SelectGameObject(gameObject->GetID());
                BeginRename(*gameObject);
            }
            
            // TODO: there are some issues with the current implementation of right-click context menu, 
            // For example, only the last GO's menu can be correctly opened. Right-clicking on the other GOs will open the blank space menu instead.
            anyGoMenuOpened = TryDrawRightClickGOMenu(*gameObject);
            ImGui::PopID();
        }

        // only show blank space menu if no GO menu is opened, otherwise the blank space menu will interfere with GO menu
        if (!anyGoMenuOpened)
        {
            TryDrawRightClickBlankSpaceMenu();
        }

        ImGui::End();
    }

    void HierarchyWindow::TryCaptureF2RenameRequest()
    {
        // Rename on F2 key pressed while the window is focused
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            if (GameObject* selected = m_Editor.GetSelectedGameObject())
            {
                BeginRename(*selected);
            }
        }
    }

    void HierarchyWindow::BeginRename(const GameObject &gameObject)
    {
        m_RenamingGameObjectId = gameObject.GetID();
        std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
        std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
        m_RequestRenameFocus = true;
    }

    bool HierarchyWindow::TryDrawRightClickBlankSpaceMenu()
    {
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Create Empty"))
            {
                m_Editor.AddEmptyGOToScene();
            }
            ImGui::EndPopup();
            return true;
        }
        return false;
    }

    bool HierarchyWindow::TryDrawRightClickGOMenu(GameObject &gameObject)
    {
        if (ImGui::BeginPopupContextItem())
        {
            m_Editor.SelectGameObject(gameObject.GetID());
            if (ImGui::MenuItem("Rename"))
            {
                BeginRename(gameObject);
            }
            if (ImGui::MenuItem("Delete"))
            {
                m_Editor.RemoveGameObjectFromScene(gameObject.GetID());
            }
            ImGui::EndPopup();
            return true;
        }
        return false;
    }
}