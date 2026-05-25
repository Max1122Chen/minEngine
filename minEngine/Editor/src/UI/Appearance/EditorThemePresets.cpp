#include "UI/Appearance/EditorThemePresets.h"

#include "UI/Property/EditorColorConversion.h"

namespace minEngine
{
    namespace
    {
        LinearColor FromDisplayRgb(float r, float g, float b, float a = 1.0f)
        {
            return EditorColorConversion::FromSrgbEditColor(EditorSrgbEditColor{r, g, b, a});
        }

        void SetIfExplicit(EditorThemePalette& out,
                           const EditorThemePalette& base,
                           const EditorThemePalette& overrides,
                           LinearColor EditorThemePalette::*field)
        {
            if (EditorThemePresets::IsTokenExplicitlySet(overrides.*field))
            {
                out.*field = overrides.*field;
            }
            else
            {
                out.*field = base.*field;
            }
        }
    }

    bool EditorThemePresets::IsTokenExplicitlySet(const LinearColor& token)
    {
        return token.R != 0.0f || token.G != 0.0f || token.B != 0.0f || token.A != 0.0f;
    }

    EditorThemePalette EditorThemePresets::MergePalettes(const EditorThemePalette& base,
                                                         const EditorThemePalette& overrides)
    {
        EditorThemePalette merged = base;
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::WindowBackground);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::PanelBackground);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::PopupBackground);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::FieldBackground);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::FieldBackgroundHovered);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::FieldBackgroundActive);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::Accent);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::AccentHovered);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::AccentActive);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::Button);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::ButtonHovered);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::ButtonActive);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::TextPrimary);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::TextMuted);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::Border);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::Separator);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::Tab);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::TabHovered);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::TabActive);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::TabUnfocused);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::TabUnfocusedActive);
        SetIfExplicit(merged, base, overrides, &EditorThemePalette::Selection);
        return merged;
    }

    EditorThemePalette EditorThemePresets::GetDarkEnginePreset()
    {
        // Neutral dark tool chrome (VS Code Dark+, Rider Darcula, Unity Editor dark — achromatic, low saturation).
        EditorThemePalette palette{};
        palette.WindowBackground = FromDisplayRgb(0.118f, 0.118f, 0.118f);           // #1e1e1e
        palette.PanelBackground = FromDisplayRgb(0.145f, 0.145f, 0.149f);           // #252526
        palette.PopupBackground = FromDisplayRgb(0.176f, 0.176f, 0.176f);           // #2d2d2d
        palette.FieldBackground = FromDisplayRgb(0.235f, 0.235f, 0.235f);           // #3c3c3c
        palette.FieldBackgroundHovered = FromDisplayRgb(0.271f, 0.271f, 0.271f);    // #454545
        palette.FieldBackgroundActive = FromDisplayRgb(0.306f, 0.306f, 0.306f);     // #4e4e4e
        palette.Accent = FromDisplayRgb(0.200f, 0.200f, 0.200f);                     // section header
        palette.AccentHovered = FromDisplayRgb(0.247f, 0.247f, 0.247f);             // #3f3f3f
        palette.AccentActive = FromDisplayRgb(0.220f, 0.220f, 0.220f);
        palette.Button = FromDisplayRgb(0.290f, 0.290f, 0.290f);                     // #4a4a4a
        palette.ButtonHovered = FromDisplayRgb(0.333f, 0.333f, 0.333f);             // #555
        palette.ButtonActive = FromDisplayRgb(0.251f, 0.251f, 0.251f);              // #404040
        palette.TextPrimary = FromDisplayRgb(0.800f, 0.800f, 0.800f);             // #ccc
        palette.TextMuted = FromDisplayRgb(0.522f, 0.522f, 0.522f);                 // #858585
        palette.Border = FromDisplayRgb(0.271f, 0.271f, 0.271f);
        palette.Separator = FromDisplayRgb(0.247f, 0.247f, 0.247f);
        palette.Tab = FromDisplayRgb(0.165f, 0.165f, 0.165f);
        palette.TabHovered = FromDisplayRgb(0.220f, 0.220f, 0.220f);
        palette.TabActive = FromDisplayRgb(0.118f, 0.118f, 0.118f);                 // match window
        palette.TabUnfocused = FromDisplayRgb(0.145f, 0.145f, 0.145f);
        palette.TabUnfocusedActive = FromDisplayRgb(0.176f, 0.176f, 0.176f);
        palette.Selection = FromDisplayRgb(0.239f, 0.239f, 0.239f);                   // #3d3d3d highlight
        return palette;
    }

    EditorThemePalette EditorThemePresets::GetLightEnginePreset()
    {
        EditorThemePalette palette{};
        palette.WindowBackground = FromDisplayRgb(0.94f, 0.94f, 0.96f);
        palette.PanelBackground = FromDisplayRgb(0.98f, 0.98f, 0.99f);
        palette.PopupBackground = FromDisplayRgb(0.98f, 0.98f, 0.99f);
        palette.FieldBackground = FromDisplayRgb(1.0f, 1.0f, 1.0f);
        palette.FieldBackgroundHovered = FromDisplayRgb(0.95f, 0.96f, 0.98f);
        palette.FieldBackgroundActive = FromDisplayRgb(0.90f, 0.92f, 0.96f);
        palette.Accent = FromDisplayRgb(0.88f, 0.91f, 0.95f);
        palette.AccentHovered = FromDisplayRgb(0.82f, 0.87f, 0.93f);
        palette.AccentActive = FromDisplayRgb(0.78f, 0.84f, 0.91f);
        palette.Button = FromDisplayRgb(0.26f, 0.45f, 0.77f);
        palette.ButtonHovered = FromDisplayRgb(0.32f, 0.52f, 0.86f);
        palette.ButtonActive = FromDisplayRgb(0.22f, 0.40f, 0.70f);
        palette.TextPrimary = FromDisplayRgb(0.12f, 0.14f, 0.18f);
        palette.TextMuted = FromDisplayRgb(0.45f, 0.48f, 0.52f);
        palette.Border = FromDisplayRgb(0.78f, 0.80f, 0.84f);
        palette.Separator = FromDisplayRgb(0.82f, 0.84f, 0.88f);
        palette.Tab = FromDisplayRgb(0.90f, 0.91f, 0.93f);
        palette.TabHovered = FromDisplayRgb(0.84f, 0.88f, 0.94f);
        palette.TabActive = FromDisplayRgb(0.96f, 0.97f, 0.99f);
        palette.TabUnfocused = FromDisplayRgb(0.88f, 0.89f, 0.91f);
        palette.TabUnfocusedActive = FromDisplayRgb(0.92f, 0.93f, 0.95f);
        palette.Selection = FromDisplayRgb(0.55f, 0.72f, 0.96f);
        return palette;
    }

    EditorThemePalette EditorThemePresets::GetPreset(std::string_view presetId)
    {
        if (presetId == EditorThemePresetIds::LightEngine)
        {
            return GetLightEnginePreset();
        }

        return GetDarkEnginePreset();
    }

    EditorThemePalette EditorThemePresets::ResolvePalette(const EditorAppearanceSettings& settings)
    {
        const std::string_view presetId = settings.ThemePresetId.empty()
            ? EditorThemePresetIds::DarkEngine
            : std::string_view(settings.ThemePresetId);

        if (presetId == EditorThemePresetIds::Custom)
        {
            const EditorThemePalette base = GetDarkEnginePreset();
            return MergePalettes(base, settings.CustomPalette);
        }

        return GetPreset(presetId);
    }
}
