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

            ImGui::TextDisabled("Double-click or F2 to rename");
            ImGui::Separator();

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

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.19f, 0.28f, 0.40f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.24f, 0.35f, 0.50f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.32f, 0.46f, 1.0f));
                    const bool committed = ImGui::InputText("##Rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                                                            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::PopStyleColor(3);

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
                    m_Editor.SelectGameObject(gameObject->m_ID);
                }

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
