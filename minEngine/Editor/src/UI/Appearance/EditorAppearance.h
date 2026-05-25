#pragma once

#include "Runtime/Function/Framework/Project/EditorAppearanceSettings.h"
#include "Runtime/Function/Framework/Project/EditorThemePalette.h"

namespace minEngine
{
    class EditorAppearance
    {
    public:
        void ApplyDefaultTheme();
        void LoadFromAppearanceSettings(const EditorAppearanceSettings& settings);
        void ApplyResolvedPalette(const EditorThemePalette& palette);

        bool SetThemePreset(std::string_view presetId, bool persistToProjectSettings);
        const EditorThemePalette& GetActivePalette() const { return m_ActivePalette; }
        const EditorAppearanceSettings& GetAppearanceSettings() const { return m_Settings; }

    private:
        static void ApplyStyleConstants();
        static void ApplyPaletteToImGui(const EditorThemePalette& palette);

        EditorAppearanceSettings m_Settings{};
        EditorThemePalette m_ActivePalette{};
    };
}
