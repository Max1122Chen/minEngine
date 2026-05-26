#pragma once

#include "Runtime/Function/Framework/Project/EditorAppearanceSettings.h"
#include "Runtime/Function/Framework/Project/EditorThemePalette.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include <array>
#include <memory>
#include <vector>

struct ImFont;

namespace minEngine
{
    class Font;

    class EditorAppearance
    {
    public:
        void ApplyDefaultTheme();
        void LoadFromAppearanceSettings(const EditorAppearanceSettings& settings);
        void ApplyResolvedPalette(const EditorThemePalette& palette);

        bool SetThemePreset(std::string_view presetId, bool persistToProjectSettings);
        const EditorThemePalette& GetActivePalette() const { return m_ActivePalette; }
        const EditorAppearanceSettings& GetAppearanceSettings() const { return m_Settings; }

        void RebuildUiFontAtlas();
        ImFont* GetImFont(EditorTypographyRole role) const;
        ImFont* GetBodyImFont() const;

    private:
        static void ApplyStyleConstants();
        static void ApplyPaletteToImGui(const EditorThemePalette& palette);

        void EnsureTypographySettings();
        std::shared_ptr<Font> ResolveFontForRole(EditorTypographyRole role) const;
        float ResolveSizePixelsForRole(EditorTypographyRole role) const;

        EditorAppearanceSettings m_Settings{};
        EditorThemePalette m_ActivePalette{};
        std::array<ImFont*, static_cast<size_t>(EditorTypographyRole::Count)> m_RoleFonts{};
        std::vector<std::shared_ptr<Font>> m_PinnedFontsForAtlas;
        bool m_UiFontBackendReady = false;
    };
}
