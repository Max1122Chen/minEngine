#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Project/EditorThemePalette.h"
#include "Runtime/Function/Framework/Project/EditorTypographySettings.h"

namespace minEngine
{
    ME_STRUCT()
    struct EditorAppearanceSettings
    {
        ME_GENERATED_BODY(EditorAppearanceSettings)

        /** "DarkEngine", "LightEngine", or "Custom". Empty uses DarkEngine. */
        ME_PROPERTY()
        std::string ThemePresetId;

        /** When ThemePresetId is "Custom", non-zero tokens override the DarkEngine base. */
        ME_PROPERTY()
        EditorThemePalette CustomPalette{};

        /** Per-role UI fonts and sizes (Body, Heading, ...). */
        ME_PROPERTY()
        EditorTypographySettings Typography{};
    };
}

#include "EditorAppearanceSettings.gen.h"
