#pragma once

#include "Runtime/Core/Math/Color.h"

namespace minEngine
{
    /** Fixed semantic colors per theme preset; not serialized in CustomPalette. */
    struct EditorSemanticColors
    {
        LinearColor LogTrace{};
        LinearColor LogDebug{};
        LinearColor LogInfo{};
        LinearColor LogWarn{};
        LinearColor LogError{};
        LinearColor LogCritical{};

        LinearColor DiagnosticInfo{};
        LinearColor DiagnosticWarning{};
        LinearColor DiagnosticError{};

        LinearColor HierarchySelectionHeader{};
        LinearColor HierarchySelectionHeaderHovered{};
        LinearColor HierarchySelectionHeaderActive{};
        LinearColor HierarchySelectionBar{};
    };
}
