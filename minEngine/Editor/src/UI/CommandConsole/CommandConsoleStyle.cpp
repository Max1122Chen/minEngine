#include "UI/CommandConsole/CommandConsoleStyle.h"

namespace minEngine
{
    CommandConsoleStyle::CommandConsoleStyle(const EditorAppearance& appearance)
        : m_Appearance(appearance)
    {
    }

    ImVec4 CommandConsoleStyle::GetColor(Command::CommandOutputKind kind) const
    {
        const EditorSemanticColors& colors = m_Appearance.GetSemanticColors();
        const EditorThemePalette& palette = m_Appearance.GetActivePalette();

        switch (kind)
        {
            case Command::CommandOutputKind::InputEcho:
                return m_Appearance.GetDisplayColor(palette.TextPrimary);
            case Command::CommandOutputKind::SuccessStatus:
                return m_Appearance.GetDisplayColor(colors.LogInfo);
            case Command::CommandOutputKind::Error:
                return m_Appearance.GetDisplayColor(colors.LogError);
            case Command::CommandOutputKind::Warning:
                return m_Appearance.GetDisplayColor(colors.LogWarn);
            case Command::CommandOutputKind::Hint:
                return m_Appearance.GetDisplayColor(colors.DiagnosticWarning);
            case Command::CommandOutputKind::Path:
                return m_Appearance.GetDisplayColor(colors.DiagnosticInfo);
            case Command::CommandOutputKind::ValueLiteral:
            case Command::CommandOutputKind::InspectValue:
                return m_Appearance.GetDisplayColor(colors.LogDebug);
            case Command::CommandOutputKind::InspectType:
            case Command::CommandOutputKind::ListItemMeta:
            case Command::CommandOutputKind::Muted:
                return m_Appearance.GetDisplayColor(palette.TextMuted);
            case Command::CommandOutputKind::InspectHeader:
            case Command::CommandOutputKind::InspectSection:
            case Command::CommandOutputKind::InspectKey:
            case Command::CommandOutputKind::ListItemName:
            case Command::CommandOutputKind::Plain:
            default:
                return m_Appearance.GetDisplayColor(palette.TextPrimary);
        }
    }

    ImVec4 CommandConsoleStyle::GetInputValidationColor(Command::PropertyValueValidationState state) const
    {
        switch (state)
        {
            case Command::PropertyValueValidationState::Valid:
                return GetColor(Command::CommandOutputKind::SuccessStatus);
            case Command::PropertyValueValidationState::Partial:
                return GetColor(Command::CommandOutputKind::Hint);
            case Command::PropertyValueValidationState::Invalid:
                return GetColor(Command::CommandOutputKind::Error);
            case Command::PropertyValueValidationState::None:
            default:
                return m_Appearance.GetDisplayColor(m_Appearance.GetActivePalette().TextPrimary);
        }
    }

    CommandCompletionRowStyle CommandConsoleStyle::GetCompletionRowStyle(bool selected) const
    {
        const EditorSemanticColors& colors = m_Appearance.GetSemanticColors();
        const EditorThemePalette& palette = m_Appearance.GetActivePalette();

        CommandCompletionRowStyle rowStyle;
        rowStyle.LabelColor = selected ? m_Appearance.GetDisplayColor(palette.TextPrimary)
                                       : m_Appearance.GetDisplayColor(palette.TextMuted);
        rowStyle.DescriptionColor = m_Appearance.GetDisplayColor(palette.TextMuted);
        rowStyle.SelectionBar = m_Appearance.GetDisplayColorU32(colors.HierarchySelectionBar);

        ImVec4 selectionBackground = m_Appearance.GetDisplayColor(colors.HierarchySelectionBar);
        selectionBackground.w = 0.22f;
        rowStyle.SelectionBackground = ImGui::ColorConvertFloat4ToU32(selectionBackground);
        return rowStyle;
    }
}
