#pragma once

#include "Core.h"

namespace minEngine
{
    ME_STRUCT()
    struct EditorSettings
    {
        ME_GENERATED_BODY(EditorSettings)

        /** 0 = use engine default (see EditorSettingsDefaults.h). */
        ME_PROPERTY()
        uint32_t MaxUndoStackDepth = 0;
    };
}

#include "EditorSettings.gen.h"
