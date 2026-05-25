#pragma once

#include "UI/Property/PropertyRefTypes.h"

#include <functional>

namespace minEngine
{
    struct PropertyRefPickerResult
    {
        bool ValueChanged = false;
        bool SelectionChanged = false;
        const PropertyRefCandidate* Selected = nullptr;
    };

    class PropertyRefPicker
    {
    public:
        static PropertyRefPickerResult DrawCombo(
            const std::vector<PropertyRefCandidate>& candidates,
            GUID currentGuid,
            float itemWidth,
            const std::function<void()>& onComboActivated = {});
    };
}
