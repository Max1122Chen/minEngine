#pragma once

#include "Runtime/Function/Framework/Project/EditorAppearanceSettings.h"
#include "Runtime/Function/Framework/Project/EditorThemePalette.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "UI/Appearance/EditorSemanticColors.h"
#include "UI/Appearance/EditorThemeColorRole.h"

#include <array>
#include <filesystem>
#include <memory>
#include <vector>

struct ImFont;
struct ImFontAtlas;
struct ImVec4;
typedef unsigned int ImU32;
typedef unsigned short ImWchar;

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
        bool SetCjkGlyphsEnabled(bool enabled, bool persistToProjectSettings);
        const EditorThemePalette& GetActivePalette() const { return m_ActivePalette; }
        const EditorSemanticColors& GetSemanticColors() const { return m_SemanticColors; }
        const EditorAppearanceSettings& GetAppearanceSettings() const { return m_Settings; }

        ImVec4 GetDisplayColor(const LinearColor& token, float alphaScale = 1.0f) const;
        ImU32 GetDisplayColorU32(const LinearColor& token, float alphaScale = 1.0f) const;
        ImVec4 GetThemeColor(EditorThemeColorRole role, float alphaScale = 1.0f) const;
        int PushThemeColors(EditorThemeColorRole role, float alphaScale = 1.0f) const;
        void PopThemeColors(int count) const;

        void RebuildUiFontAtlas();
        ImFont* GetImFont(EditorTypographyRole role) const;
        ImFont* GetBodyImFont() const;
        ImFont* GetAssetIconRegularImFont() const { return m_AssetIconRegularFont; }
        ImFont* GetAssetIconSolidImFont() const { return m_AssetIconSolidFont; }

    private:
        static void ApplyStyleConstants();
        static void ApplyPaletteToImGui(const EditorThemePalette& palette);

        void EnsureTypographySettings();
        bool PersistAppearanceSettingsToProject();
        const ImWchar* BuildUiGlyphRanges(ImFontAtlas& fontAtlas);
        std::shared_ptr<Font> ResolveFontForRole(EditorTypographyRole role) const;
        std::filesystem::path ResolveAssetIconFontPath() const;
        float ResolveSizePixelsForRole(EditorTypographyRole role) const;

        EditorAppearanceSettings m_Settings{};
        EditorThemePalette m_ActivePalette{};
        EditorSemanticColors m_SemanticColors{};
        std::array<ImFont*, static_cast<size_t>(EditorTypographyRole::Count)> m_RoleFonts{};
        ImFont* m_AssetIconRegularFont = nullptr;
        ImFont* m_AssetIconSolidFont = nullptr;
        std::vector<std::shared_ptr<Font>> m_PinnedFontsForAtlas;
        bool m_UiFontBackendReady = false;
    };
}
