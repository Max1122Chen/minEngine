#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

#include <string>
#include <vector>

namespace minEngine::Reflection
{
    class MEClass;
}

namespace minEngine::Command
{
    struct ResolvedPropertyTarget
    {
        void* OwnerObject = nullptr;
        const Reflection::MEClass* OwnerClass = nullptr;
        std::string PropertySubPath;
    };

    struct PropertySetTransaction
    {
        GUID OwnerGuid{};
        std::string OwnerClassName;
        std::string PropertySubPath;
        std::vector<uint8_t> BeforeValue;
        std::vector<uint8_t> AfterValue;
    };

    struct PropertyPathSuggestion
    {
        std::string Label;
        std::string InsertText;
        std::string TypeName;
    };

    enum class PropertyPathResolveStatus : uint8_t
    {
        Ok,
        NotFound,
        Ambiguous,
    };
}
