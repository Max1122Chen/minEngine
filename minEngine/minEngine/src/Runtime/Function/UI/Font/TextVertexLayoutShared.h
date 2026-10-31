#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    /**
     * The vertex layout every ScreenUI text draw reads its vertices through.
     *
     * One handle for the process. The text builder creates it on first use and gives it back at
     * render teardown, and a text graphic hands that same handle to the widget that draws it, so a
     * proxy keeps the layout alive for exactly as long as it draws with it.
     *
     * Declared here rather than in TextMeshBuilder.h because only this module's drawing code needs
     * it: a consumer outside the module would get a handle of its own instead of this one, and it
     * would then bind a layout nobody else has. For the same reason it is one function rather than
     * a header-inline accessor - an inline local static is one object per module, not per process.
     */
    RHIVertexInputLayoutRef& SharedTextVertexInputLayout();
}
