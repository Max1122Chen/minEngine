#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Reflection/MEClass.h"

#include <memory>
#include <string>

namespace minEngine
{
    class AssetMeta;
    class MEObject;

    enum class PropertyRefCandidateKind : uint8_t
    {
        AssetMeta,
        LiveObject,
    };

    struct PropertyRefCandidate
    {
        PropertyRefCandidateKind Kind = PropertyRefCandidateKind::AssetMeta;
        GUID Guid;
        std::string DisplayName;
        const AssetMeta* Meta = nullptr;
        std::shared_ptr<MEObject> Object;
    };

    using AllowedClasses = std::vector<const Reflection::MEClass*>;
}
