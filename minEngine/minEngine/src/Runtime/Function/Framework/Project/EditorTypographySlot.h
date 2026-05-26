#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

namespace minEngine
{
    ME_STRUCT()
    struct EditorTypographySlot
    {
        ME_GENERATED_BODY(EditorTypographySlot)

        /** Zero uses built-in default font resolution for the role. */
        ME_PROPERTY()
        GUID FontAssetGuid{};

        /** Baked atlas size in pixels; zero uses role default from EditorTypographyDefaults. */
        ME_PROPERTY()
        float SizePixels = 0.0f;
    };
}

#include "EditorTypographySlot.gen.h"
