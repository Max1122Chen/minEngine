#pragma once

#include <string>

#include "imgui.h"

#include "IPanel.h"

namespace minEngine
{
    class HierarchyPanel final : public IPanel
    {
    public:
        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw(const PanelContext& context) override
        {
            if (context.state == nullptr)
            {
                return;
            }

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 8.0f, viewport->Pos.y + 44.0f), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(260.0f, viewport->Size.y - 280.0f), ImGuiCond_Once);

            ImGui::Begin(m_Title.c_str());
            for (int i = 0; i < static_cast<int>(context.state->hierarchyItems.size()); ++i)
            {
                const bool selected = (context.state->selectedHierarchyIndex == i);
                if (ImGui::Selectable(context.state->hierarchyItems[i].c_str(), selected))
                {
                    context.state->selectedHierarchyIndex = i;
                    context.state->inspectorName = context.state->hierarchyItems[i];
                }
            }
            ImGui::End();
        }

    private:
        const std::string m_Id = "hierarchy";
        const std::string m_Title = "Hierarchy";
    };
}
