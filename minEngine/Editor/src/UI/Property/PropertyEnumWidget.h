#pragma once

/**
 * Scaffold only — not wired into PropertyValueWidget yet.
 * Blocked: reflection does not record enum underlying type/size; current storage
 * heuristics are unsafe. Re-enable after MEPrimitiveProperty carries enum layout.
 */

#include "Runtime/Core/Reflection/MEProperties.h"

namespace minEngine::Reflection
{
    class MEEnum;
}

namespace minEngine
{
    /** Combo UI for reflected ME_ENUM fields (primitiveTypeName = registered enum type). */
    class PropertyEnumWidget
    {
    public:
        static bool Draw(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr, float itemWidth = -1.0f);

    private:
        static size_t GetEnumStorageSize(const minEngine::Reflection::MEEnum& enumInfo);
        static int64_t ReadEnumValue(void* propertyPtr, size_t storageSize);
        static void WriteEnumValue(void* propertyPtr, size_t storageSize, int64_t value);
    };
}
