#pragma once

#include <string>

#include "imgui.h"

#include "IPanel.h"

namespace minEngine
{
    class InspectorPanel final : public IPanel
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

            ImGui::Begin(m_Title.c_str());
            ImGui::Text("Selected: %s", context.state->inspectorName.c_str());
            ImGui::Separator();
            ImGui::DragFloat3("Position", context.state->inspectorPosition, 0.05f);
            ImGui::DragFloat3("Rotation", context.state->inspectorRotation, 0.5f);
            ImGui::DragFloat3("Scale", context.state->inspectorScale, 0.05f, 0.01f, 100.0f);
            ImGui::ColorEdit3("Tint", context.state->inspectorTint);
            ImGui::End();
        }

    private:
        const std::string m_Id = "inspector";
        const std::string m_Title = "Inspector";
    };
}
