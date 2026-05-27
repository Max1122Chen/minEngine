#include "UI/Property/PropertyRefPicker.h"

#include "imgui.h"

namespace minEngine
{
    PropertyRefPickerResult PropertyRefPicker::DrawCombo(
        const std::vector<PropertyRefCandidate>& candidates,
        GUID currentGuid,
        float itemWidth,
        const std::function<void()>& onComboActivated)
    {
        PropertyRefPickerResult result;

        const PropertyRefCandidate* currentCandidate = nullptr;
        for (const PropertyRefCandidate& candidate : candidates)
        {
            if (candidate.Guid == currentGuid)
            {
                currentCandidate = &candidate;
                break;
            }
        }

        std::string previewLabel = "None";
        if (currentCandidate != nullptr)
        {
            previewLabel = currentCandidate->DisplayName;
        }
        else if (!currentGuid.IsZero())
        {
            previewLabel = "Invalid";
        }

        if (itemWidth != 0.0f)
        {
            ImGui::SetNextItemWidth(itemWidth);
        }

        const bool comboOpened = ImGui::BeginCombo("##ObjectPtrRefCombo", previewLabel.c_str());
        if (onComboActivated && ImGui::IsItemActivated())
        {
            onComboActivated();
        }

        if (!comboOpened)
        {
            return result;
        }

        const bool noneSelected = currentCandidate == nullptr && currentGuid.IsZero();
        if (ImGui::Selectable("None", noneSelected))
        {
            result.ValueChanged = true;
            result.SelectionChanged = !noneSelected;
            result.Selected = nullptr;
        }

        for (const PropertyRefCandidate& candidate : candidates)
        {
            const bool isSelected = currentCandidate == &candidate;
            ImGui::PushID(static_cast<int>(candidate.Guid.High ^ candidate.Guid.Low));
            if (ImGui::Selectable(candidate.DisplayName.c_str(), isSelected))
            {
                result.ValueChanged = true;
                result.SelectionChanged = candidate.Guid != currentGuid;
                result.Selected = &candidate;
            }
            ImGui::PopID();

            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
        return result;
    }
}
