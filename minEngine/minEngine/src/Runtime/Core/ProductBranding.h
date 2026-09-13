#pragma once

#include "EngineAPI.h"

#include <string>

namespace minEngine
{
    // User-facing product display name (WF-F03). Not the C++ namespace / repo path.
    inline constexpr const char* kProductDisplayName = "Maximum";

    // Shipping editor process stem (no extension). Avoid bare "Maximum" — BUG-EDITOR-003.
    inline constexpr const char* kProductEditorExecutableStem = "MaximumEditor";

    inline const char* GetProductDisplayName()
    {
        return kProductDisplayName;
    }

    inline const char* GetProductEditorExecutableStem()
    {
        return kProductEditorExecutableStem;
    }

    // Window title base, e.g. "Maximum Editor".
    inline std::string FormatEditorWindowTitleBase()
    {
        return std::string(kProductDisplayName) + " Editor";
    }

    // Empty documentSuffix → base only; otherwise "Maximum Editor - <suffix>".
    inline std::string FormatEditorWindowTitle(const std::string& documentSuffix)
    {
        std::string title = FormatEditorWindowTitleBase();
        if (!documentSuffix.empty())
        {
            title += " - ";
            title += documentSuffix;
        }
        return title;
    }

    // CLI --version / About primary line, e.g. "Maximum 0.0.9".
    inline std::string FormatProductVersionLine(const std::string& versionString)
    {
        return std::string(kProductDisplayName) + " " + versionString;
    }
}
