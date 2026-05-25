#include "UI/Property/PropertyPrimitiveWidgets.h"

#include "imgui.h"

#include "Runtime/Core/Math/Math.h"

#include <cstring>
#include <string>

namespace minEngine
{
    std::string PropertyPrimitiveWidgets::GetShortTypeName(const std::string& fullTypeName)
    {
        const size_t colon = fullTypeName.rfind(':');
        if (colon != std::string::npos && colon + 1 < fullTypeName.size())
        {
            return fullTypeName.substr(colon + 1);
        }

        return fullTypeName;
    }

    bool PropertyPrimitiveWidgets::IsSignedIntegerTypeName(const std::string& shortTypeName)
    {
        return shortTypeName == "int"
            || shortTypeName == "int32"
            || shortTypeName == "int16"
            || shortTypeName == "long"
            || shortTypeName == "int64";
    }

    bool PropertyPrimitiveWidgets::Draw(const Reflection::MEPrimitiveProperty& primitiveProperty,
                                        void* propertyPtr,
                                        float itemWidth)
    {
        const std::string shortTypeName = GetShortTypeName(primitiveProperty.primitiveTypeName);
        if (itemWidth != 0.0f)
        {
            ImGui::SetNextItemWidth(itemWidth);
        }

        if (IsSignedIntegerTypeName(shortTypeName))
        {
            return ImGui::DragInt("##Value", static_cast<int*>(propertyPtr), 1);
        }

        if (shortTypeName == "uint32")
        {
            return ImGui::DragScalar(
                "##Value",
                ImGuiDataType_U32,
                propertyPtr,
                1.0f,
                nullptr,
                nullptr,
                "%u");
        }

        if (shortTypeName == "float")
        {
            return ImGui::DragFloat("##Value", static_cast<float*>(propertyPtr), 0.1f);
        }

        if (shortTypeName == "double")
        {
            return ImGui::DragScalar("##Value", ImGuiDataType_Double, propertyPtr, 0.1f);
        }

        if (shortTypeName == "bool")
        {
            return ImGui::Checkbox("##Value", static_cast<bool*>(propertyPtr));
        }

        if (shortTypeName == "string" || shortTypeName == "std::string")
        {
            std::string* stringValue = static_cast<std::string*>(propertyPtr);
            char textBuffer[256] = {};
            std::strncpy(textBuffer, stringValue->c_str(), sizeof(textBuffer) - 1);
            if (ImGui::InputText("##Value", textBuffer, sizeof(textBuffer)))
            {
                *stringValue = textBuffer;
                return true;
            }

            return false;
        }

        if (shortTypeName == "Vector2")
        {
            Vector2* value = static_cast<Vector2*>(propertyPtr);
            float data[2] = {value->x, value->y};
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
            const bool changed = ImGui::DragFloat2("##Value", data, 0.1f);
            ImGui::PopStyleVar(2);
            if (changed)
            {
                value->x = data[0];
                value->y = data[1];
            }

            return changed;
        }

        if (shortTypeName == "Vector3")
        {
            Vector3* value = static_cast<Vector3*>(propertyPtr);
            float data[3] = {value->x, value->y, value->z};
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
            const bool changed = ImGui::DragFloat3("##Value", data, 0.1f);
            ImGui::PopStyleVar(2);
            if (changed)
            {
                value->x = data[0];
                value->y = data[1];
                value->z = data[2];
            }

            return changed;
        }

        if (shortTypeName == "Vector4")
        {
            Vector4* value = static_cast<Vector4*>(propertyPtr);
            float data[4] = {value->x, value->y, value->z, value->w};
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
            const bool changed = ImGui::DragFloat4("##Value", data, 0.1f);
            ImGui::PopStyleVar(2);
            if (changed)
            {
                value->x = data[0];
                value->y = data[1];
                value->z = data[2];
                value->w = data[3];
            }

            return changed;
        }

        ImGui::TextDisabled("Unsupported: %s", shortTypeName.c_str());
        return false;
    }
}
