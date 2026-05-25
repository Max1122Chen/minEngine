#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

#include <cstdint>
#include <functional>
#include <string>

namespace minEngine
{
    enum class AssetRegistryChangeKind : uint8_t
    {
        Registered = 0,
        Unregistered = 1,
        Moved = 2,
        MetaUpdated = 3,
        Reimported = 4
    };

    struct AssetRegistryChange
    {
        AssetRegistryChangeKind Kind = AssetRegistryChangeKind::Registered;
        GUID Guid{};
        std::string OldPath;
        std::string NewPath;
        std::string AssetTypeId;
    };

    using AssetRegistryChangedCallback = std::function<void(const AssetRegistryChange&)>;

    constexpr uint32_t kInvalidAssetRegistrySubscriptionId = 0u;
}
