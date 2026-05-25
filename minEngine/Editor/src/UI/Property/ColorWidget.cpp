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
        float rgba[4] = {srgbEdit.R, srgbEdit.G, srgbEdit.B, srgbEdit.A};
        if (!ImGui::ColorEdit4("##Value", rgba, ImGuiColorEditFlags_Float))
        {
            return false;
        }

        srgbEdit.R = rgba[0];
        srgbEdit.G = rgba[1];
        srgbEdit.B = rgba[2];
        srgbEdit.A = rgba[3];
        const LinearColor newLinear = EditorColorConversion::FromSrgbEditColor(srgbEdit);
        if (newLinear == *value)
        {
            return false;
        }

        *value = newLinear;
        return true;
    }
}
