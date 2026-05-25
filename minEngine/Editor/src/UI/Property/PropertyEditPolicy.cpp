#include "UI/Property/PropertyEditPolicy.h"

#include "Runtime/Core/Reflection/MEProperties.h"

namespace minEngine
{
    namespace
    {
        using Reflection::PropertySpecifier;

        bool IsDefaultsContext(EditorPropertyEditContextKind contextKind)
        {
            return contextKind == EditorPropertyEditContextKind::AssetDefaults;
        }

        bool IsSceneInstanceContext(EditorPropertyEditContextKind contextKind)
        {
            return contextKind == EditorPropertyEditContextKind::SceneInstance;
        }

        bool MetadataIsTrue(const Reflection::MEProperty& property, const char* key)
        {
            const std::string* value = property.FindMetadata(key);
            if (value == nullptr)
            {
                return false;
            }

            return *value == "true" || *value == "1" || *value == "True";
        }
    }

    bool PropertyEditPolicy::ShouldShow(const Reflection::MEProperty& property,
                                        EditorPropertyEditContextKind contextKind)
    {
        if (property.HasSpecifier(PropertySpecifier::Invisible))
        {
            return false;
        }

        if (property.HasSpecifier(PropertySpecifier::VisibleDefaultsOnly) && !IsDefaultsContext(contextKind))
        {
            return false;
        }

        if (property.HasSpecifier(PropertySpecifier::VisibleInstanceOnly) && !IsSceneInstanceContext(contextKind))
        {
            return false;
        }

        return true;
    }

    bool PropertyEditPolicy::CanEdit(const Reflection::MEProperty& property, EditorPropertyEditContextKind contextKind)
    {
        if (!ShouldShow(property, contextKind))
        {
            return false;
        }

        if (property.HasSpecifier(PropertySpecifier::Transient))
        {
            return false;
        }

        if (property.HasSpecifier(PropertySpecifier::EditDefaultsOnly) && !IsDefaultsContext(contextKind))
        {
            return false;
        }

        if (property.HasSpecifier(PropertySpecifier::EditInstanceOnly) && !IsSceneInstanceContext(contextKind))
        {
            return false;
        }

        if (MetadataIsTrue(property, "ReadOnly"))
        {
            return false;
        }

        return true;
    }

    bool PropertyEditPolicy::IsReadOnly(const Reflection::MEProperty& property, EditorPropertyEditContextKind contextKind)
    {
        return ShouldShow(property, contextKind) && !CanEdit(property, contextKind);
    }

    const char* PropertyEditPolicy::GetDisplayName(const Reflection::MEProperty& property)
    {
        if (const std::string* displayName = property.FindMetadata("DisplayName"))
        {
            return displayName->c_str();
        }

        return property.GetName().c_str();
    }

    const char* PropertyEditPolicy::GetTooltip(const Reflection::MEProperty& property)
    {
        if (const std::string* tooltip = property.FindMetadata("Tooltip"))
        {
            return tooltip->c_str();
        }

        return nullptr;
    }
}
