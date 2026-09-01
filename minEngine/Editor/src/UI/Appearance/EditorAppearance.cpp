#include "UI/Appearance/EditorAppearance.h"

#include "Platform/EditorImGuiBackend.h"
#include "UI/Appearance/EditorThemePresets.h"
#include "UI/Appearance/EditorTypographyDefaults.h"
#include "UI/Property/EditorColorConversion.h"

#include "imgui.h"
#include "IconFontCppHeaders/IconsFontAwesome7.h"

#include "Log/LogSystem.h"
#include "Resource/AssetManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Resource/Font.h"

namespace minEngine
{
    namespace
    {
        constexpr const char* kAssetIconRegularFontRelativePath = "Fonts/Font Awesome 7 Free-Regular-400.otf";
        constexpr const char* kAssetIconSolidFontRelativePath = "Fonts/Font Awesome 7 Free-Solid-900.otf";
        constexpr float kAssetIconFontSizePixels = 40.0f;

        ImVec4 ToImGuiColor(const LinearColor& linear)
        {
            return EditorColorConversion::ToImGuiDisplayColor(linear);
        }

        size_t RoleIndex(EditorTypographyRole role)
        {
            return static_cast<size_t>(role);
        }

        ImVector<ImWchar> s_BuiltUiGlyphRanges;
    }

    void EditorAppearance::ApplyStyleConstants()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 7.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 6.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 6.0f;
        style.ScrollbarRounding = 8.0f;

        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(9.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 7.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    }

    void EditorAppearance::ApplyPaletteToImGui(const EditorThemePalette& palette)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        colors[ImGuiCol_WindowBg] = ToImGuiColor(palette.WindowBackground);
        colors[ImGuiCol_ChildBg] = ToImGuiColor(palette.PanelBackground);
        colors[ImGuiCol_PopupBg] = ToImGuiColor(palette.PopupBackground);
        colors[ImGuiCol_Border] = ToImGuiColor(palette.Border);
        colors[ImGuiCol_Separator] = ToImGuiColor(palette.Separator);

        colors[ImGuiCol_FrameBg] = ToImGuiColor(palette.FieldBackground);
        colors[ImGuiCol_FrameBgHovered] = ToImGuiColor(palette.FieldBackgroundHovered);
        colors[ImGuiCol_FrameBgActive] = ToImGuiColor(palette.FieldBackgroundActive);

        colors[ImGuiCol_Header] = ToImGuiColor(palette.Accent);
        colors[ImGuiCol_HeaderHovered] = ToImGuiColor(palette.AccentHovered);
        colors[ImGuiCol_HeaderActive] = ToImGuiColor(palette.AccentActive);

        colors[ImGuiCol_Button] = ToImGuiColor(palette.Button);
        colors[ImGuiCol_ButtonHovered] = ToImGuiColor(palette.ButtonHovered);
        colors[ImGuiCol_ButtonActive] = ToImGuiColor(palette.ButtonActive);

        colors[ImGuiCol_Tab] = ToImGuiColor(palette.Tab);
        colors[ImGuiCol_TabHovered] = ToImGuiColor(palette.TabHovered);
        colors[ImGuiCol_TabSelected] = ToImGuiColor(palette.TabActive);
        colors[ImGuiCol_TabSelectedOverline] = ToImGuiColor(palette.Border);
        colors[ImGuiCol_TabDimmed] = ToImGuiColor(palette.TabUnfocused);
        colors[ImGuiCol_TabDimmedSelected] = ToImGuiColor(palette.TabUnfocusedActive);
        colors[ImGuiCol_TabDimmedSelectedOverline] = ToImGuiColor(palette.Border);

        colors[ImGuiCol_TitleBg] = ToImGuiColor(palette.Tab);
        colors[ImGuiCol_TitleBgActive] = ToImGuiColor(palette.PanelBackground);
        colors[ImGuiCol_TitleBgCollapsed] = ToImGuiColor(palette.TabUnfocused);

        colors[ImGuiCol_MenuBarBg] = ToImGuiColor(palette.PanelBackground);
        colors[ImGuiCol_DockingEmptyBg] = ToImGuiColor(palette.WindowBackground);
        colors[ImGuiCol_DockingPreview] = ToImGuiColor(palette.Selection);

        colors[ImGuiCol_Text] = ToImGuiColor(palette.TextPrimary);
        colors[ImGuiCol_TextDisabled] = ToImGuiColor(palette.TextMuted);
    }

    ImVec4 EditorAppearance::GetDisplayColor(const LinearColor& token, float alphaScale) const
    {
        ImVec4 color = EditorColorConversion::ToImGuiDisplayColor(token);
        if (alphaScale != 1.0f)
        {
            color.w *= alphaScale;
        }

        return color;
    }

    ImU32 EditorAppearance::GetDisplayColorU32(const LinearColor& token, float alphaScale) const
    {
        return ImGui::ColorConvertFloat4ToU32(GetDisplayColor(token, alphaScale));
    }

    ImVec4 EditorAppearance::GetThemeColor(EditorThemeColorRole role, float alphaScale) const
    {
        const EditorThemePalette& palette = m_ActivePalette;
        const EditorSemanticColors& semantic = m_SemanticColors;

        switch (role)
        {
            case EditorThemeColorRole::Field:
                return GetDisplayColor(palette.FieldBackground, alphaScale);
            case EditorThemeColorRole::SectionHeader:
                return GetDisplayColor(palette.Accent, alphaScale);
            case EditorThemeColorRole::SubduedSectionHeader:
                return GetDisplayColor(palette.Accent, alphaScale);
            case EditorThemeColorRole::PanelOverlay:
                return GetDisplayColor(palette.PanelBackground, alphaScale);
            case EditorThemeColorRole::PrimaryText:
                return GetDisplayColor(palette.TextPrimary, alphaScale);
            case EditorThemeColorRole::HierarchySelection:
                return GetDisplayColor(semantic.HierarchySelectionHeader, alphaScale);
            default:
                return GetDisplayColor(palette.TextPrimary, alphaScale);
        }
    }

    int EditorAppearance::PushThemeColors(EditorThemeColorRole role, float alphaScale) const
    {
        const EditorThemePalette& palette = m_ActivePalette;
        const EditorSemanticColors& semantic = m_SemanticColors;

        switch (role)
        {
            case EditorThemeColorRole::Field:
                ImGui::PushStyleColor(ImGuiCol_FrameBg, GetDisplayColor(palette.FieldBackground, alphaScale));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, GetDisplayColor(palette.FieldBackgroundHovered, alphaScale));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, GetDisplayColor(palette.FieldBackgroundActive, alphaScale));
                return 3;

            case EditorThemeColorRole::SectionHeader:
                ImGui::PushStyleColor(ImGuiCol_Header, GetDisplayColor(palette.Accent, alphaScale));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GetDisplayColor(palette.AccentHovered, alphaScale));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, GetDisplayColor(palette.AccentActive, alphaScale));
                return 3;

            case EditorThemeColorRole::SubduedSectionHeader:
            {
                const float subduedAlpha = alphaScale * 0.35f;
                const float subduedHoverAlpha = alphaScale * 0.50f;
                ImGui::PushStyleColor(ImGuiCol_Header, GetDisplayColor(palette.Accent, subduedAlpha));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GetDisplayColor(palette.AccentHovered, subduedHoverAlpha));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, GetDisplayColor(palette.AccentActive, subduedHoverAlpha));
                return 3;
            }

            case EditorThemeColorRole::PanelOverlay:
                ImGui::PushStyleColor(ImGuiCol_ChildBg, GetDisplayColor(palette.PanelBackground, alphaScale));
                ImGui::PushStyleColor(ImGuiCol_Border, GetDisplayColor(palette.Border, 0.95f));
                return 2;

            case EditorThemeColorRole::PrimaryText:
                ImGui::PushStyleColor(ImGuiCol_Text, GetDisplayColor(palette.TextPrimary, alphaScale));
                return 1;

            case EditorThemeColorRole::HierarchySelection:
                ImGui::PushStyleColor(ImGuiCol_Header, GetDisplayColor(semantic.HierarchySelectionHeader, alphaScale));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                      GetDisplayColor(semantic.HierarchySelectionHeaderHovered, alphaScale));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                      GetDisplayColor(semantic.HierarchySelectionHeaderActive, alphaScale));
                return 3;

            default:
                return 0;
        }
    }

    void EditorAppearance::PopThemeColors(int count) const
    {
        if (count > 0)
        {
            ImGui::PopStyleColor(count);
        }
    }

    void EditorAppearance::ApplyResolvedPalette(const EditorThemePalette& palette)
    {
        m_ActivePalette = palette;
        m_SemanticColors = EditorThemePresets::GetSemanticColors(m_Settings.ThemePresetId);
        if (m_Settings.ThemePresetId == std::string(EditorThemePresetIds::LightEngine))
        {
            ImGui::StyleColorsLight();
        }
        else
        {
            ImGui::StyleColorsDark();
        }

        ApplyStyleConstants();
        ApplyPaletteToImGui(palette);
    }

    void EditorAppearance::EnsureTypographySettings()
    {
        EditorTypographyDefaults::ApplyBuiltinDefaults(m_Settings.Typography);
    }

    void EditorAppearance::ApplyDefaultTheme()
    {
        m_Settings.ThemePresetId = std::string(EditorThemePresetIds::DarkEngine);
        EnsureTypographySettings();
        ApplyResolvedPalette(EditorThemePresets::GetDarkEnginePreset());
    }

    void EditorAppearance::LoadFromAppearanceSettings(const EditorAppearanceSettings& settings)
    {
        m_Settings = settings;
        if (m_Settings.ThemePresetId.empty())
        {
            m_Settings.ThemePresetId = std::string(EditorThemePresetIds::DarkEngine);
        }

        EnsureTypographySettings();
        ApplyResolvedPalette(EditorThemePresets::ResolvePalette(m_Settings));

        if (m_UiFontBackendReady)
        {
            RebuildUiFontAtlas();
        }
    }

    bool EditorAppearance::PersistAppearanceSettingsToProject()
    {
        ProjectManager& projectManager = ProjectManager::Get();
        if (!projectManager.HasOpenProject())
        {
            return false;
        }

        projectManager.GetCurrentProjectCtx().Settings.Appearance = m_Settings;
        return projectManager.SaveCurrentProjectSettings();
    }

    bool EditorAppearance::SetThemePreset(std::string_view presetId, bool persistToProjectSettings)
    {
        m_Settings.ThemePresetId = std::string(presetId);
        ApplyResolvedPalette(EditorThemePresets::ResolvePalette(m_Settings));

        if (!persistToProjectSettings)
        {
            return true;
        }

        return PersistAppearanceSettingsToProject();
    }

    bool EditorAppearance::SetCjkGlyphsEnabled(bool enabled, bool persistToProjectSettings)
    {
        if (m_Settings.Typography.bEnableCjkGlyphs == enabled)
        {
            return true;
        }

        m_Settings.Typography.bEnableCjkGlyphs = enabled;
        if (m_UiFontBackendReady)
        {
            RebuildUiFontAtlas();
        }

        if (!persistToProjectSettings)
        {
            return true;
        }

        return PersistAppearanceSettingsToProject();
    }

    const ImWchar* EditorAppearance::BuildUiGlyphRanges(ImFontAtlas& fontAtlas)
    {
        if (!m_Settings.Typography.bEnableCjkGlyphs)
        {
            return fontAtlas.GetGlyphRangesDefault();
        }

        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(fontAtlas.GetGlyphRangesDefault());
        builder.AddRanges(fontAtlas.GetGlyphRangesChineseFull());
        s_BuiltUiGlyphRanges.clear();
        builder.BuildRanges(&s_BuiltUiGlyphRanges);
        return s_BuiltUiGlyphRanges.Data;
    }

    float EditorAppearance::ResolveSizePixelsForRole(EditorTypographyRole role) const
    {
        const size_t index = RoleIndex(role);
        if (index < m_Settings.Typography.Slots.size() && m_Settings.Typography.Slots[index].SizePixels > 0.0f)
        {
            return m_Settings.Typography.Slots[index].SizePixels;
        }

        return EditorTypographyDefaults::GetDefaultSizePixels(role);
    }

    std::shared_ptr<Font> EditorAppearance::ResolveFontForRole(EditorTypographyRole role) const
    {
        AssetManager& assetManager = AssetManager::Get();
        const size_t index = RoleIndex(role);

        GUID fontGuid = GUID::Zero();
        if (index < m_Settings.Typography.Slots.size())
        {
            fontGuid = m_Settings.Typography.Slots[index].FontAssetGuid;
        }

        if (fontGuid.IsZero())
        {
            fontGuid = EditorTypographyDefaults::GetDefaultFontAssetGuid(role);
        }

        if (fontGuid.IsValid())
        {
            std::string errorMessage;
            std::shared_ptr<Asset> asset = assetManager.LoadAssetByGUID(fontGuid, errorMessage);
            if (std::shared_ptr<Font> font = std::dynamic_pointer_cast<Font>(asset))
            {
                return font;
            }

            ME_CORE_WARN(
                "EditorAppearance: failed to load font GUID {} for role {} ({}).",
                fontGuid.ToString(),
                static_cast<int>(role),
                errorMessage);
        }

        const char* projectRelativePath = EditorTypographyDefaults::GetDefaultFontProjectPath(role);
        const AssetMeta* meta = assetManager.FindAssetMetaByPath(projectRelativePath);
        if (meta != nullptr)
        {
            return assetManager.LoadAsset<Font>(meta->AssetPath);
        }

        if (role != EditorTypographyRole::Body)
        {
            return ResolveFontForRole(EditorTypographyRole::Body);
        }

        return nullptr;
    }

    void EditorAppearance::FinalizeFontAtlasBuild()
    {
        ImGuiIO& io = ImGui::GetIO();
        const bool rendererHasTextures =
            (io.BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0;
        if (!rendererHasTextures)
        {
            io.Fonts->Build();
        }

        if (m_ImGuiBackend != nullptr)
        {
            m_ImGuiBackend->NotifyFontAtlasRebuilt();
        }
    }

    std::filesystem::path EditorAppearance::ResolveAssetIconFontPath() const
    {
        const std::filesystem::path& engineDefaultAssetsRoot = PathRegistry::Get().GetEngineDefaultAssetsRoot();
        if (engineDefaultAssetsRoot.empty())
        {
            return {};
        }

        return std::filesystem::weakly_canonical(engineDefaultAssetsRoot / kAssetIconRegularFontRelativePath);
    }

    void EditorAppearance::RebuildUiFontAtlas()
    {
        if (!ImGui::GetCurrentContext())
        {
            return;
        }

        m_UiFontBackendReady = true;
        m_RoleFonts.fill(nullptr);
        m_AssetIconRegularFont = nullptr;
        m_AssetIconSolidFont = nullptr;
        m_PinnedFontsForAtlas.clear();

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        io.FontGlobalScale = 1.0f;

        std::shared_ptr<Font> bodyFont = ResolveFontForRole(EditorTypographyRole::Body);
        if (bodyFont == nullptr || !bodyFont->IsValid())
        {
            ME_CORE_WARN("EditorAppearance: no valid Body font; falling back to ImGui default font.");
            ImFontConfig defaultConfig;
            defaultConfig.FontDataOwnedByAtlas = true;
            m_RoleFonts[RoleIndex(EditorTypographyRole::Body)] =
                io.Fonts->AddFontDefault(&defaultConfig);
            for (size_t roleIndex = 1; roleIndex < m_RoleFonts.size(); ++roleIndex)
            {
                m_RoleFonts[roleIndex] = m_RoleFonts[RoleIndex(EditorTypographyRole::Body)];
            }
            FinalizeFontAtlasBuild();
            return;
        }

        const ImWchar* glyphRanges = BuildUiGlyphRanges(*io.Fonts);

        for (size_t roleIndex = 0; roleIndex < m_RoleFonts.size(); ++roleIndex)
        {
            const auto role = static_cast<EditorTypographyRole>(roleIndex);
            std::shared_ptr<Font> font = ResolveFontForRole(role);
            if (font == nullptr || !font->IsValid())
            {
                font = bodyFont;
                ME_CORE_WARN(
                    "EditorAppearance: role {} uses Body font fallback.",
                    static_cast<int>(role));
            }

            const float sizePixels = ResolveSizePixelsForRole(role);
            const std::vector<uint8_t>& fontBytes = font->GetFontFileBytes();

            m_PinnedFontsForAtlas.push_back(font);

            ImFontConfig fontConfig;
            fontConfig.FontDataOwnedByAtlas = false;

            ImFont* bakedFont = io.Fonts->AddFontFromMemoryTTF(
                const_cast<uint8_t*>(fontBytes.data()),
                static_cast<int>(fontBytes.size()),
                sizePixels,
                &fontConfig,
                glyphRanges);

            if (bakedFont == nullptr)
            {
                ME_CORE_ERROR(
                    "EditorAppearance: AddFontFromMemoryTTF failed for role {} (size {}).",
                    static_cast<int>(role),
                    sizePixels);
                continue;
            }

            m_RoleFonts[roleIndex] = bakedFont;
        }

        if (m_RoleFonts[RoleIndex(EditorTypographyRole::Body)] == nullptr)
        {
            ME_CORE_WARN("EditorAppearance: Body font bake failed; using ImGui default.");
            m_RoleFonts[RoleIndex(EditorTypographyRole::Body)] = io.Fonts->AddFontDefault();
        }

        {
            const std::filesystem::path iconFontPath = ResolveAssetIconFontPath();
            if (iconFontPath.empty())
            {
                ME_CORE_WARN("EditorAppearance: EngineDefaultAssetsRoot is empty, icon font will be unavailable.");
            }
            else
            {
                static const ImWchar iconGlyphRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
                ImFontConfig iconFontConfig;
                iconFontConfig.MergeMode = false;
                iconFontConfig.PixelSnapH = true;
                iconFontConfig.GlyphMinAdvanceX = 22.0f;
                m_AssetIconRegularFont = io.Fonts->AddFontFromFileTTF(
                    iconFontPath.string().c_str(),
                    kAssetIconFontSizePixels,
                    &iconFontConfig,
                    iconGlyphRanges);
                if (m_AssetIconRegularFont == nullptr)
                {
                    ME_CORE_WARN(
                        "EditorAppearance: failed to load regular icon font '{}'.",
                        iconFontPath.string());
                }
            }
        }

        {
            const std::filesystem::path& engineDefaultAssetsRoot = PathRegistry::Get().GetEngineDefaultAssetsRoot();
            if (engineDefaultAssetsRoot.empty())
            {
                ME_CORE_WARN("EditorAppearance: EngineDefaultAssetsRoot is empty, solid icon font will be unavailable.");
            }
            else
            {
                const std::filesystem::path solidIconFontPath =
                    std::filesystem::weakly_canonical(engineDefaultAssetsRoot / kAssetIconSolidFontRelativePath);
                static const ImWchar iconGlyphRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
                ImFontConfig iconFontConfig;
                iconFontConfig.MergeMode = false;
                iconFontConfig.PixelSnapH = true;
                iconFontConfig.GlyphMinAdvanceX = 22.0f;
                m_AssetIconSolidFont = io.Fonts->AddFontFromFileTTF(
                    solidIconFontPath.string().c_str(),
                    kAssetIconFontSizePixels,
                    &iconFontConfig,
                    iconGlyphRanges);
                if (m_AssetIconSolidFont == nullptr)
                {
                    ME_CORE_WARN(
                        "EditorAppearance: failed to load solid icon font '{}'.",
                        solidIconFontPath.string());
                }
            }
        }

        ImFont* bodyImFont = m_RoleFonts[RoleIndex(EditorTypographyRole::Body)];
        for (ImFont*& roleFont : m_RoleFonts)
        {
            if (roleFont == nullptr)
            {
                roleFont = bodyImFont;
            }
        }

        FinalizeFontAtlasBuild();

        ME_CORE_INFO("EditorAppearance: UI font atlas rebuilt ({} roles, regularIconReady={}, solidIconReady={}).",
                     m_RoleFonts.size(),
                     m_AssetIconRegularFont != nullptr,
                     m_AssetIconSolidFont != nullptr);
    }

    ImFont* EditorAppearance::GetImFont(EditorTypographyRole role) const
    {
        const size_t index = RoleIndex(role);
        if (index >= m_RoleFonts.size())
        {
            return nullptr;
        }

        return m_RoleFonts[index];
    }

    ImFont* EditorAppearance::GetBodyImFont() const
    {
        return GetImFont(EditorTypographyRole::Body);
    }
}
