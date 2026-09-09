#pragma once

#include "Core.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

namespace minEngine
{
    /** One ScreenUI draw item. Never enqueue into Opaque/Translucent. */
    struct UIDrawCommand
    {
        MeshDrawCommand Draw;
        uint32_t StableOrder = 0;
        int32_t CanvasSortOrder = 0;
    };
}
