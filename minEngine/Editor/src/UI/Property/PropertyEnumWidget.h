#pragma once

#include "Runtime/Core/Reflection/MEProperties.h"

namespace minEngine
{
    /** Combo UI for reflected ME_ENUM fields (uses MEPrimitiveProperty::GetEnum / GetSize). */
    class PropertyEnumWidget
    {
    public:
        static bool Draw(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr, float itemWidth = -1.0f);

    private:
        static int64_t ReadEnumValue(void* propertyPtr, size_t storageSize);
        static void WriteEnumValue(void* propertyPtr, size_t storageSize, int64_t value);
    };
}
