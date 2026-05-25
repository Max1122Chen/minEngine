#include "UI/Appearance/EditorAppearance.h"

#include "UI/Appearance/EditorThemePresets.h"
#include "UI/Property/EditorColorConversion.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/ProjectManager.h"

namespace minEngine
{
    namespace
    {
        ImVec4 ToImGuiColor(const LinearColor& linear)
        {
            return EditorColorConversion::ToImGuiDisplayColor(linear);
        }
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

    void EditorAppearance::ApplyResolvedPalette(const EditorThemePalette& palette)
    {
        m_ActivePalette = palette;
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

    void EditorAppearance::ApplyDefaultTheme()
    {
        m_Settings.ThemePresetId = std::string(EditorThemePresetIds::DarkEngine);
        ApplyResolvedPalette(EditorThemePresets::GetDarkEnginePreset());
    }

    void EditorAppearance::LoadFromAppearanceSettings(const EditorAppearanceSettings& settings)
    {
        m_Settings = settings;
        if (m_Settings.ThemePresetId.empty())
        {
            m_Settings.ThemePresetId = std::string(EditorThemePresetIds::DarkEngine);
        }

        ApplyResolvedPalette(EditorThemePresets::ResolvePalette(m_Settings));
    }

    bool EditorAppearance::SetThemePreset(std::string_view presetId, bool persistToProjectSettings)
    {
        m_Settings.ThemePresetId = std::string(presetId);
        ApplyResolvedPalette(EditorThemePresets::ResolvePalette(m_Settings));

        if (!persistToProjectSettings)
        {
            return true;
        }

        ProjectManager& projectManager = ProjectManager::Get();
        if (!projectManager.HasOpenProject())
        {
            return false;
        }

        projectManager.GetCurrentProjectCtx().Settings.Appearance = m_Settings;
        return projectManager.SaveCurrentProjectSettings();
    }
}
