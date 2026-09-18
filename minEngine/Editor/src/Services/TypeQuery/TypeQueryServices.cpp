#include "Services/TypeQuery/TypeQueryServices.h"

#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine
{
    std::vector<const Reflection::MEClass*> TypeQueryServices::QueryClasses(
        const Reflection::MEClass* base,
        Reflection::ClassSpecifierMask requireAll,
        Reflection::ClassSpecifierMask excludeAny) const
    {
        std::vector<const Reflection::MEClass*> matched;
        const std::vector<const Reflection::MEClass*> allClasses =
            Reflection::ReflectionSystem::Get().GetAllClasses();
        matched.reserve(allClasses.size());
        for (const Reflection::MEClass* classInfo : allClasses)
        {
            if (classInfo == nullptr)
            {
                continue;
            }
            if (base != nullptr && !classInfo->IsA(base))
            {
                continue;
            }
            const Reflection::ClassSpecifierMask mask = classInfo->GetSpecifierMask();
            if ((mask & requireAll) != requireAll)
            {
                continue;
            }
            if ((mask & excludeAny) != 0)
            {
                continue;
            }
            matched.push_back(classInfo);
        }
        return matched;
    }

    std::vector<const Reflection::MEProperty*> TypeQueryServices::QueryProperties(
        const Reflection::MEClass& owner,
        Reflection::PropertySpecifierMask requireAll,
        Reflection::PropertySpecifierMask excludeAny) const
    {
        std::vector<const Reflection::MEProperty*> matched;
        Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
            &owner,
            [&](const Reflection::MEProperty& property) -> bool
            {
                const Reflection::PropertySpecifierMask mask = property.GetSpecifierMask();
                if ((mask & requireAll) != requireAll)
                {
                    return true;
                }
                if ((mask & excludeAny) != 0)
                {
                    return true;
                }
                matched.push_back(&property);
                return true;
            });
        return matched;
    }

    bool TypeQueryServices::TryParseClassSpecifierMask(
        const std::vector<std::string>& names,
        Reflection::ClassSpecifierMask& outMask,
        std::string& outError) const
    {
        outMask = 0;
        for (const std::string& name : names)
        {
            if (name == "Abstract")
            {
                outMask |= static_cast<Reflection::ClassSpecifierMask>(Reflection::ClassSpecifier::Abstract);
            }
            else if (name == "Transient")
            {
                outMask |= static_cast<Reflection::ClassSpecifierMask>(Reflection::ClassSpecifier::Transient);
            }
            else if (name == "EditorOnly")
            {
                outMask |= static_cast<Reflection::ClassSpecifierMask>(Reflection::ClassSpecifier::EditorOnly);
            }
            else if (name == "ScriptType")
            {
                outMask |= static_cast<Reflection::ClassSpecifierMask>(Reflection::ClassSpecifier::ScriptType);
            }
            else
            {
                outError = "Unknown class specifier '" + name + "'.";
                return false;
            }
        }
        return true;
    }

    bool TypeQueryServices::TryParsePropertySpecifierMask(
        const std::vector<std::string>& names,
        Reflection::PropertySpecifierMask& outMask,
        std::string& outError) const
    {
        outMask = 0;
        for (const std::string& name : names)
        {
            Reflection::PropertySpecifier specifier = Reflection::PropertySpecifier::None;
            if (name == "Transient")
            {
                specifier = Reflection::PropertySpecifier::Transient;
            }
            else if (name == "EditAnywhere")
            {
                specifier = Reflection::PropertySpecifier::EditAnywhere;
            }
            else if (name == "EditDefaultsOnly")
            {
                specifier = Reflection::PropertySpecifier::EditDefaultsOnly;
            }
            else if (name == "EditInstanceOnly")
            {
                specifier = Reflection::PropertySpecifier::EditInstanceOnly;
            }
            else if (name == "VisibleAnywhere")
            {
                specifier = Reflection::PropertySpecifier::VisibleAnywhere;
            }
            else if (name == "VisibleDefaultsOnly")
            {
                specifier = Reflection::PropertySpecifier::VisibleDefaultsOnly;
            }
            else if (name == "VisibleInstanceOnly")
            {
                specifier = Reflection::PropertySpecifier::VisibleInstanceOnly;
            }
            else if (name == "Invisible")
            {
                specifier = Reflection::PropertySpecifier::Invisible;
            }
            else if (name == "Instanced")
            {
                specifier = Reflection::PropertySpecifier::Instanced;
            }
            else if (name == "ScriptReadOnly")
            {
                specifier = Reflection::PropertySpecifier::ScriptReadOnly;
            }
            else if (name == "ScriptReadWrite")
            {
                specifier = Reflection::PropertySpecifier::ScriptReadWrite;
            }
            else if (name == "ScriptAssignable")
            {
                specifier = Reflection::PropertySpecifier::ScriptAssignable;
            }
            else
            {
                outError = "Unknown property specifier '" + name + "'.";
                return false;
            }
            outMask |= static_cast<Reflection::PropertySpecifierMask>(specifier);
        }
        return true;
    }

    void TypeQueryServices::AppendClassSpecifierNames(
        Reflection::ClassSpecifierMask mask,
        std::vector<std::string>& outNames) const
    {
        const Reflection::ClassSpecifier known[] = {
            Reflection::ClassSpecifier::Abstract,
            Reflection::ClassSpecifier::Transient,
            Reflection::ClassSpecifier::EditorOnly,
            Reflection::ClassSpecifier::ScriptType,
        };
        const char* names[] = {"Abstract", "Transient", "EditorOnly", "ScriptType"};
        for (size_t index = 0; index < 4; ++index)
        {
            const auto bit = static_cast<Reflection::ClassSpecifierMask>(known[index]);
            if ((mask & bit) != 0)
            {
                outNames.emplace_back(names[index]);
            }
        }
    }

    void TypeQueryServices::AppendPropertySpecifierNames(
        Reflection::PropertySpecifierMask mask,
        std::vector<std::string>& outNames) const
    {
        const Reflection::PropertySpecifier known[] = {
            Reflection::PropertySpecifier::Transient,
            Reflection::PropertySpecifier::EditAnywhere,
            Reflection::PropertySpecifier::EditDefaultsOnly,
            Reflection::PropertySpecifier::EditInstanceOnly,
            Reflection::PropertySpecifier::VisibleAnywhere,
            Reflection::PropertySpecifier::VisibleDefaultsOnly,
            Reflection::PropertySpecifier::VisibleInstanceOnly,
            Reflection::PropertySpecifier::Invisible,
            Reflection::PropertySpecifier::Instanced,
            Reflection::PropertySpecifier::ScriptReadOnly,
            Reflection::PropertySpecifier::ScriptReadWrite,
            Reflection::PropertySpecifier::ScriptAssignable,
        };
        const char* names[] = {
            "Transient",
            "EditAnywhere",
            "EditDefaultsOnly",
            "EditInstanceOnly",
            "VisibleAnywhere",
            "VisibleDefaultsOnly",
            "VisibleInstanceOnly",
            "Invisible",
            "Instanced",
            "ScriptReadOnly",
            "ScriptReadWrite",
            "ScriptAssignable",
        };
        for (size_t index = 0; index < 12; ++index)
        {
            const auto bit = static_cast<Reflection::PropertySpecifierMask>(known[index]);
            if ((mask & bit) != 0)
            {
                outNames.emplace_back(names[index]);
            }
        }
    }

    std::string TypeQueryServices::DescribePropertyType(const Reflection::MEProperty& property)
    {
        if (const auto* primitive = dynamic_cast<const Reflection::MEPrimitiveProperty*>(&property))
        {
            return primitive->primitiveTypeName;
        }
        if (const auto* objectProperty = dynamic_cast<const Reflection::MEObjectProperty*>(&property))
        {
            if (objectProperty->GetValueClass() != nullptr)
            {
                return objectProperty->GetValueClass()->GetName();
            }
        }
        return {};
    }
}
