#pragma once

#include "Core.h"

#include <cstdint>
#include <vector>

namespace minEngine
{
    enum class HierarchyHitKind
    {
        Blank,
        GameObjectItem,
    };

    struct HierarchyMenuContext
    {
        HierarchyHitKind HitKind = HierarchyHitKind::Blank;
        bool bClickedEmpty = false;
        std::vector<uint64_t> SelectedGameObjectIds;
    };

} // namespace minEngine
