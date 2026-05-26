#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Project/EditorTypographySlot.h"

#include <vector>

namespace minEngine
{
    ME_STRUCT()
    struct EditorTypographySettings
    {
        ME_GENERATED_BODY(EditorTypographySettings)

        /** Indexed by EditorTypographyRole; size should match EditorTypographyRole::Count. */
        ME_PROPERTY()
        std::vector<EditorTypographySlot> Slots;

        /** M5.1: when true, all roles merge CJK glyph ranges during atlas build. */
        ME_PROPERTY()
        bool bEnableCjkGlyphs = false;
    };
}

#include "EditorTypographySettings.gen.h"
