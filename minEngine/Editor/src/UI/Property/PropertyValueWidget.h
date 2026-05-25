#pragma once

#include "Runtime/Core/Reflection/MEProperties.h"

namespace minEngine
{
    /** Dispatches property value UI to shared widgets (M3). */
    class PropertyValueWidget
    {
    public:
        static bool Draw(const Reflection::MEProperty& property,
                         void* propertyPtr,
                         float itemWidth = -1.0f);

        static bool IsLinearColorStruct(const Reflection::MEClass* valueClass);
    };
}
