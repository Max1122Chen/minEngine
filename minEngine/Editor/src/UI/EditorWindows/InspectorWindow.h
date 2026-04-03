#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

namespace minEngine
{
    class InspectorWindow final : public EditorWindow
    {
    public:
        explicit InspectorWindow(Editor& editor)
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
            float position[3] = {m_Editor.inspectorPosition[0], m_Editor.inspectorPosition[1], m_Editor.inspectorPosition[2]};
            float rotation[3] = {m_Editor.inspectorRotation[0], m_Editor.inspectorRotation[1], m_Editor.inspectorRotation[2]};
            float scale[3] = {m_Editor.inspectorScale[0], m_Editor.inspectorScale[1], m_Editor.inspectorScale[2]};
            float tint[3] = {m_Editor.inspectorTint[0], m_Editor.inspectorTint[1], m_Editor.inspectorTint[2]};

            ImGui::Begin(m_Title.c_str());
            ImGui::Text("Selected: %s", m_Editor.inspectorName.c_str());
            ImGui::Separator();
            ImGui::DragFloat3("Position", position, 0.05f);
            ImGui::DragFloat3("Rotation", rotation, 0.5f);
            ImGui::DragFloat3("Scale", scale, 0.05f, 0.01f, 100.0f);
            ImGui::ColorEdit3("Tint", tint);
            ImGui::End();
        }

    private:
        const std::string m_Id = "inspector";
        const std::string m_Title = "Inspector";
    };
}
