#include "HierarchyWindow.h"

#include "Shell/EditorContextHelpers.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemeScope.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTheme.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

namespace minEngine
{
    void HierarchyWindow::OnDraw()
    {
        if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str()))
        {
            return;
        }

        {
            EditorAppearance& appearance = m_Context.GetEditorAppearance();
            EditorTypographyScope bodyTypography(appearance, EditorTypographyRole::Body);

            if (ImGui::Button("Create Empty"))
            {
                GetSceneEditor(&m_Context)->SubmitAddEmptyGOToScene(m_Context);
            }
            ImGui::Separator();

            const std::vector<GameObject*> gameObjects = GetSceneEditor(&m_Context)->GetHierarchyGameObjects();
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

                {
                    EditorThemeScope renameFieldTheme = EditorWindowTheme::Field(appearance);
                    const bool committed = ImGui::InputText("##Rename",
                                                            m_RenameBuffer,
                                                            sizeof(m_RenameBuffer),
                                                            ImGuiInputTextFlags_AutoSelectAll |
                                                                ImGuiInputTextFlags_EnterReturnsTrue);

                    if (committed || ImGui::IsItemDeactivatedAfterEdit())
                    {
                        GetSceneEditor(&m_Context)->SubmitRenameGameObject(
                            m_Context, gameObject->GetID(), m_RenameBuffer);
                        m_RenamingGameObjectId = kInvalidGameObjectId;
                    }
                    else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                    {
                        m_RenamingGameObjectId = kInvalidGameObjectId;
                    }
                }

                ImGui::PopID();
                continue;
            }

            const bool selected = GetSceneEditor(&m_Context)->IsGameObjectSelected(gameObject->GetID());
            const std::string displayName = GetSceneEditor(&m_Context)->GetGameObjectDisplayName(*gameObject);
            const std::string label = std::string("  ") + displayName;

            if (selected)
            {
                {
                    EditorThemeScope selectionTheme = EditorWindowTheme::HierarchySelection(appearance);
                    if (ImGui::Selectable(label.c_str(), true, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        GetSceneEditor(&m_Context)->SelectGameObject(gameObject->GetID());
                    }

                    const ImVec2 minPos = ImGui::GetItemRectMin();
                    const ImVec2 maxPos = ImGui::GetItemRectMax();
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const ImU32 barColor =
                        appearance.GetDisplayColorU32(appearance.GetSemanticColors().HierarchySelectionBar);
                    drawList->AddRectFilled(ImVec2(minPos.x + 3.0f, minPos.y + 4.0f),
                                            ImVec2(minPos.x + 7.0f, maxPos.y - 4.0f),
                                            barColor,
                                            2.0f);
                }
            }
            else
            {
                if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                {
                    GetSceneEditor(&m_Context)->SelectGameObject(gameObject->GetID());
                }
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                GetSceneEditor(&m_Context)->SelectGameObject(gameObject->GetID());
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
        }

        ImGui::End();
    }

    void HierarchyWindow::TryCaptureF2RenameRequest()
    {
        // Rename on F2 key pressed while the window is focused
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            if (GameObject* selected = GetSceneEditor(&m_Context)->GetSelectedGameObject())
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
                GetSceneEditor(&m_Context)->SubmitAddEmptyGOToScene(m_Context);
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
            GetSceneEditor(&m_Context)->SelectGameObject(gameObject.GetID());
            if (ImGui::MenuItem("Rename"))
            {
                BeginRename(gameObject);
            }
            if (ImGui::MenuItem("Delete"))
            {
                GetSceneEditor(&m_Context)->SubmitRemoveGameObjectFromScene(m_Context, gameObject.GetID());
            }
            ImGui::EndPopup();
            return true;
        }
        return false;
    }
}