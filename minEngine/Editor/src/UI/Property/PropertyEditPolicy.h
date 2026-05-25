#pragma once

#include "UI/Property/PropertyEditTypes.h"

#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine
{
    class PropertyEditPolicy
    {
    public:
        static bool ShouldShow(const Reflection::MEProperty& property, EditorPropertyEditContextKind contextKind);
        static bool CanEdit(const Reflection::MEProperty& property, EditorPropertyEditContextKind contextKind);
        static bool IsReadOnly(const Reflection::MEProperty& property, EditorPropertyEditContextKind contextKind);

        static const char* GetDisplayName(const Reflection::MEProperty& property);
        static const char* GetTooltip(const Reflection::MEProperty& property);
    };
}
