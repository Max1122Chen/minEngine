#pragma once

#include "imgui.h"

namespace minEngine
{
    class IEditorContext;

    class EditorWindowTypography
    {
    public:
        /** Push Heading font only for ImGui::Begin (scope ends before return). */
        static bool BeginPanel(IEditorContext& context,
                               const char* title,
                               bool* open = nullptr,
                               ImGuiWindowFlags flags = 0);
    };
}
