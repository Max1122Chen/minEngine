#pragma once

#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Core/Reflection/MEProperties.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    /**
     * Editor-side read-only type catalog (MCP-F04).
     * Returns live reflection pointers. Callers apply their own semantics (e.g. addable components).
     */
    class TypeQueryServices
    {
    public:
        std::vector<const Reflection::MEClass*> QueryClasses(
            const Reflection::MEClass* base,
            Reflection::ClassSpecifierMask requireAll,
            Reflection::ClassSpecifierMask excludeAny) const;

        std::vector<const Reflection::MEProperty*> QueryProperties(
            const Reflection::MEClass& owner,
            Reflection::PropertySpecifierMask requireAll,
            Reflection::PropertySpecifierMask excludeAny) const;

        bool TryParseClassSpecifierMask(
            const std::vector<std::string>& names,
            Reflection::ClassSpecifierMask& outMask,
            std::string& outError) const;

        bool TryParsePropertySpecifierMask(
            const std::vector<std::string>& names,
            Reflection::PropertySpecifierMask& outMask,
            std::string& outError) const;

        void AppendClassSpecifierNames(
            Reflection::ClassSpecifierMask mask,
            std::vector<std::string>& outNames) const;

        void AppendPropertySpecifierNames(
            Reflection::PropertySpecifierMask mask,
            std::vector<std::string>& outNames) const;

        static std::string DescribePropertyType(const Reflection::MEProperty& property);
    };
}
