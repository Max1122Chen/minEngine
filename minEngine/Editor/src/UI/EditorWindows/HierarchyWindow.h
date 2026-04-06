#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <cstring>
#include <limits>

namespace minEngine
{
    class HierarchyWindow final : public EditorWindow
    {
    public:
        explicit HierarchyWindow(Editor& editor)
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
            ImGui::Begin(m_Title.c_str());

            const std::vector<std::shared_ptr<GameObject>> gameObjects = m_Editor.GetHierarchyGameObjects();
            if (gameObjects.empty())
            {
                ImGui::TextUnformatted("No GameObject in current scene.");
                ImGui::End();
                return;
            }

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2, false))
            {
                if (const std::shared_ptr<GameObject> selected = m_Editor.GetSelectedGameObject())
                {
                    BeginRename(*selected);
                }
            }

            for (const std::shared_ptr<GameObject>& gameObject : gameObjects)
            {
                if (!gameObject)
                {
                    continue;
                }

                ImGui::PushID(static_cast<int>(gameObject->m_ID));

                if (m_RenamingGameObjectId == gameObject->m_ID)
                {
                    if (m_RequestRenameFocus)
                    {
                        ImGui::SetKeyboardFocusHere();
                        m_RequestRenameFocus = false;
                    }

                    const bool committed = ImGui::InputText("##Rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                                                            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
                    if (committed || ImGui::IsItemDeactivatedAfterEdit())
                    {
                        m_Editor.RenameGameObject(gameObject->m_ID, m_RenameBuffer);
                        m_RenamingGameObjectId = kInvalidGameObjectId;
                    }
                    else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                    {
                        m_RenamingGameObjectId = kInvalidGameObjectId;
                    }

                    ImGui::PopID();
                    continue;
                }

                const bool selected = m_Editor.IsGameObjectSelected(gameObject->m_ID);
                const std::string label = m_Editor.GetGameObjectDisplayName(*gameObject);
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    m_Editor.SelectGameObject(gameObject->m_ID);
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    m_Editor.SelectGameObject(gameObject->m_ID);
                    BeginRename(*gameObject);
                }

                ImGui::PopID();
            }

            ImGui::End();
        }

    private:
        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        void BeginRename(const GameObject& gameObject)
        {
            m_RenamingGameObjectId = gameObject.m_ID;
            std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
            std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
            m_RequestRenameFocus = true;
        }

        const std::string m_Id = "hierarchy";
        const std::string m_Title = "Hierarchy";
        uint64_t m_RenamingGameObjectId = kInvalidGameObjectId;
        bool m_RequestRenameFocus = false;
        char m_RenameBuffer[256] = {};
    };
}
