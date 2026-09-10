#include "UI/Property/ColorWidget.h"

#include "UI/Property/EditorColorConversion.h"

#include "imgui.h"

namespace minEngine
{
    bool ColorWidget::DrawLinearColor(LinearColor* value, float itemWidth)
    {
        if (value == nullptr)
        {
            return false;
        }

        if (itemWidth != 0.0f)
        {
            ImGui::SetNextItemWidth(itemWidth);
        }

        EditorSrgbEditColor srgbEdit = EditorColorConversion::ToSrgbEditColor(*value);
        const ImVec4 displayColor(srgbEdit.R, srgbEdit.G, srgbEdit.B, srgbEdit.A);

        const ImGuiColorEditFlags buttonFlags =
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaPreviewHalf;
        if (ImGui::ColorButton("##Color", displayColor, buttonFlags, ImVec2(0.0f, 0.0f)))
        {
            ImGui::OpenPopup("##LinearColorPicker");
        }

        bool changed = false;
        if (ImGui::BeginPopup("##LinearColorPicker"))
        {
            float rgba[4] = {srgbEdit.R, srgbEdit.G, srgbEdit.B, srgbEdit.A};
            const ImGuiColorEditFlags pickerFlags =
                ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview
                | ImGuiColorEditFlags_NoSmallPreview;
            if (ImGui::ColorPicker4("##Picker", rgba, pickerFlags))
            {
                srgbEdit.R = rgba[0];
                srgbEdit.G = rgba[1];
                srgbEdit.B = rgba[2];
                srgbEdit.A = rgba[3];
                const LinearColor newLinear = EditorColorConversion::FromSrgbEditColor(srgbEdit);
                if (newLinear != *value)
                {
                    *value = newLinear;
                    changed = true;
                }
            }

            if (ImGui::Button("Close", ImVec2(-1.0f, 0.0f)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        return changed;
    }
}
