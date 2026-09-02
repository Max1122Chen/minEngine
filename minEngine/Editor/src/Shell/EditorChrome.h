#pragma once

#include "Core.h"

#include "imgui.h"

namespace minEngine
{
    class IEditorContext;

    /** Draws MainMenu into the ImGui main viewport work area (no extra dock host). */
    class EditorChrome
    {
    public:
        static void BeginFrame(IEditorContext& context);
    };
}
