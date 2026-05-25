// Not linked from PropertyValueWidget until reflection exposes enum storage layout.

#include "UI/Property/PropertyEnumWidget.h"

#include "imgui.h"

#include "Runtime/Core/Reflection/MEEnum.h"
#include "Runtime/Core/Reflection/Reflection.h"

#include <cstring>
#include <string>

namespace minEngine
{
    size_t PropertyEnumWidget::GetEnumStorageSize(const Reflection::MEEnum& enumInfo)
    {
        const std::string& enumName = enumInfo.GetName();
        if (enumName.find("InputKeyAction") != std::string::npos)
        {
            return sizeof(int);
        }

        return sizeof(uint8_t);
    }

    int64_t PropertyEnumWidget::ReadEnumValue(void* propertyPtr, size_t storageSize)
    {
        int64_t value = 0;
        std::memcpy(&value, propertyPtr, storageSize);
        return value;
    }

    void PropertyEnumWidget::WriteEnumValue(void* propertyPtr, size_t storageSize, int64_t value)
    {
        int64_t truncated = value;
        std::memcpy(propertyPtr, &truncated, storageSize);
    }

    bool PropertyEnumWidget::Draw(const Reflection::MEPrimitiveProperty& primitiveProperty,
                                  void* propertyPtr,
                                  float itemWidth)
    {
        const Reflection::MEEnum* enumInfo =
            Reflection::ReflectionSystem::Get().FindEnum(primitiveProperty.primitiveTypeName);
        if (enumInfo == nullptr || propertyPtr == nullptr)
        {
            return false;
        }

        if (itemWidth != 0.0f)
        {
            ImGui::SetNextItemWidth(itemWidth);
        }

        const size_t storageSize = GetEnumStorageSize(*enumInfo);
        const int64_t currentValue = ReadEnumValue(propertyPtr, storageSize);
        const Reflection::MEEnumEntry* currentEntry = enumInfo->FindByValue(currentValue);
        const char* previewLabel = currentEntry != nullptr ? currentEntry->name.c_str() : "Invalid";

        bool changed = false;
        if (ImGui::BeginCombo("##EnumValue", previewLabel))
        {
            for (const Reflection::MEEnumEntry& entry : enumInfo->GetEntries())
            {
                const bool selected = entry.value == currentValue;
                if (ImGui::Selectable(entry.name.c_str(), selected))
                {
                    WriteEnumValue(propertyPtr, storageSize, entry.value);
                    changed = true;
                }
            }

            ImGui::EndCombo();
        }

        return changed;
    }
}
