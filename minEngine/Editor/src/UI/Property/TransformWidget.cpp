#include "UI/Property/TransformWidget.h"

#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorTypographyScope.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include "Runtime/Function/Framework/Transform/Transform.h"

namespace minEngine
{
    static bool DrawVector3Row(const char* label,
                              Vector3& value,
                              const char* dragId,
                              const std::function<void(std::string_view fieldName)>& applyUndoForField,
                              std::string_view fieldName)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);

        float data[3] = {value.x, value.y, value.z};
        ImGui::PushID(dragId);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
        const bool changed = ImGui::DragFloat3("##Value", data, 0.1f);
        ImGui::PopStyleVar(2);
        ImGui::PopID();

        if (applyUndoForField)
        {
            applyUndoForField(fieldName);
        }

        if (changed)
        {
            value.x = data[0];
            value.y = data[1];
            value.z = data[2];
        }

        return changed;
    }

    bool TransformWidget::Draw(Transform* transform,
                               int treeFlags,
                               const std::function<void(std::string_view fieldName)>& applyUndoForField,
                               EditorAppearance* appearance)
    {
        if (transform == nullptr)
        {
            ImGui::TextUnformatted("Transform is null.");
            return false;
        }

        bool valueChanged = false;
        bool open = false;
        if (appearance != nullptr)
        {
            EditorTypographyScope subheadingScope(*appearance, EditorTypographyRole::Subheading);
            open = ImGui::TreeNodeEx("##TransformTree", treeFlags, "Transform");
        }
        else
        {
            open = ImGui::TreeNodeEx("##TransformTree", treeFlags, "Transform");
        }
        if (open)
        {
            const char* tableId = "##TransformNestedTable";
            if (ImGui::BeginTable(tableId, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

                valueChanged |= DrawVector3Row(
                    "Location",
                    transform->Position,
                    "Location",
                    applyUndoForField,
                    "Position");
                valueChanged |= DrawVector3Row(
                    "Rotation",
                    transform->Rotation,
                    "Rotation",
                    applyUndoForField,
                    "Rotation");
                valueChanged |= DrawVector3Row(
                    "Scale",
                    transform->Scale,
                    "Scale",
                    applyUndoForField,
                    "Scale");

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        return valueChanged;
    }
}

