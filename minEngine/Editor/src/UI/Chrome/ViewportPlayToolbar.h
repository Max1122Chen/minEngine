#pragma once

#include "Core.h"

namespace minEngine
{
    class IEditorContext;

    /** Play controls in a dedicated row inside the Scene Viewport panel (below dock tab). */
    class ViewportPlayToolbar
    {
    public:
        static float DrawToolbarRow(IEditorContext& context);
    };
}
