#pragma once

#include "Core.h"

#include <cstdint>

namespace minEngine
{
    class Component;

    enum class SceneInspectorSelectionKind
    {
        GameObjectHeader,
        Component,
    };

    struct SceneInspectorMenuContext
    {
        SceneInspectorSelectionKind SelectionKind = SceneInspectorSelectionKind::GameObjectHeader;
        uint64_t GameObjectId = 0;
        Component* HoveredComponent = nullptr;
    };

} // namespace minEngine
