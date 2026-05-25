#pragma once

#include "Core.h"

namespace minEngine
{
    /** Where the property row is being edited (drives specifier semantics). */
    enum class EditorPropertyEditContextKind : uint8_t
    {
        SceneInstance,
        /** Non-scene authoring: asset defaults, material node defs, project defaults, etc. */
        AssetDefaults,
    };
}
