#pragma once

#include "Runtime/Core/Reflection/MEProperties.h"

#include "imgui.h"

#include <string>
#include <unordered_map>

namespace minEngine
{
    /** ImGui controls for reflected primitive fields (and Vector2/3/4). */
    class PropertyPrimitiveWidgets
    {
    public:
        static bool Draw(const Reflection::MEPrimitiveProperty& primitiveProperty,
                         void* propertyPtr,
                         float itemWidth = -1.0f);

    private:
        static std::string GetShortTypeName(const std::string& fullTypeName);
        static bool IsSignedIntegerTypeName(const std::string& shortTypeName);

        /** Commit string only on IsItemDeactivatedAfterEdit (sticky draft while typing). */
        static bool DrawDeferredString(std::string* stringValue, float itemWidth);
        static std::unordered_map<ImGuiID, std::string>& ActiveStringEditBuffers();
    };
}
