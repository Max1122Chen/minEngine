#pragma once

#include "Runtime/Function/Framework/Project/EditorAppearanceSettings.h"
#include "Runtime/Function/Framework/Project/EditorThemePalette.h"
#include "UI/Appearance/EditorSemanticColors.h"

#include <string_view>

namespace minEngine
{
    namespace EditorThemePresetIds
    {
        inline constexpr std::string_view DarkEngine = "DarkEngine";
        inline constexpr std::string_view LightEngine = "LightEngine";
        inline constexpr std::string_view Custom = "Custom";
    }

    class EditorThemePresets
    {
    public:
        static EditorThemePalette GetDarkEnginePreset();
        static EditorThemePalette GetLightEnginePreset();
        static EditorThemePalette GetPreset(std::string_view presetId);
        static EditorThemePalette ResolvePalette(const EditorAppearanceSettings& settings);
        static bool IsTokenExplicitlySet(const LinearColor& token);
        static EditorThemePalette MergePalettes(const EditorThemePalette& base, const EditorThemePalette& overrides);
        static EditorSemanticColors GetSemanticColors(std::string_view presetId);
    };
}
