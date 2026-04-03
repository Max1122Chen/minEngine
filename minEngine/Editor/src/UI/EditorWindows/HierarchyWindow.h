#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

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
            for (int i = 0; i < static_cast<int>(m_Editor.hierarchyItems.size()); ++i)
            {
                const bool selected = (m_Editor.selectedHierarchyIndex == i);
                ImGui::Selectable(m_Editor.hierarchyItems[i].c_str(), selected, ImGuiSelectableFlags_Disabled);
            }
            ImGui::End();
        }

    private:
        const std::string m_Id = "hierarchy";
        const std::string m_Title = "Hierarchy";
    };
}
