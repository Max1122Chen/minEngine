#include "UI/CommandConsole/CommandConsoleStyle.h"

namespace minEngine
{
    CommandConsoleStyle::CommandConsoleStyle(const EditorAppearance& appearance)
        : m_Appearance(appearance)
    {
    }

    ImVec4 CommandConsoleStyle::GetColor(DebugCommand::DebugCommandOutputKind kind) const
    {
        const EditorSemanticColors& colors = m_Appearance.GetSemanticColors();
        const EditorThemePalette& palette = m_Appearance.GetActivePalette();

        switch (kind)
        {
            case DebugCommand::DebugCommandOutputKind::InputEcho:
                return m_Appearance.GetDisplayColor(palette.TextPrimary);
            case DebugCommand::DebugCommandOutputKind::SuccessStatus:
                return m_Appearance.GetDisplayColor(colors.LogInfo);
            case DebugCommand::DebugCommandOutputKind::Error:
                return m_Appearance.GetDisplayColor(colors.LogError);
            case DebugCommand::DebugCommandOutputKind::Warning:
                return m_Appearance.GetDisplayColor(colors.LogWarn);
            case DebugCommand::DebugCommandOutputKind::Hint:
                return m_Appearance.GetDisplayColor(colors.DiagnosticWarning);
            case DebugCommand::DebugCommandOutputKind::Path:
                return m_Appearance.GetDisplayColor(colors.DiagnosticInfo);
            case DebugCommand::DebugCommandOutputKind::ValueLiteral:
            case DebugCommand::DebugCommandOutputKind::InspectValue:
                return m_Appearance.GetDisplayColor(colors.LogDebug);
            case DebugCommand::DebugCommandOutputKind::InspectType:
            case DebugCommand::DebugCommandOutputKind::ListItemMeta:
            case DebugCommand::DebugCommandOutputKind::Muted:
                return m_Appearance.GetDisplayColor(palette.TextMuted);
            case DebugCommand::DebugCommandOutputKind::InspectHeader:
            case DebugCommand::DebugCommandOutputKind::InspectSection:
            case DebugCommand::DebugCommandOutputKind::InspectKey:
            case DebugCommand::DebugCommandOutputKind::ListItemName:
            case DebugCommand::DebugCommandOutputKind::Plain:
            default:
                return m_Appearance.GetDisplayColor(palette.TextPrimary);
        }
    }

    ImVec4 CommandConsoleStyle::GetInputValidationColor(DebugCommand::PropertyValueValidationState state) const
    {
        switch (state)
        {
            case DebugCommand::PropertyValueValidationState::Valid:
                return GetColor(DebugCommand::DebugCommandOutputKind::SuccessStatus);
            case DebugCommand::PropertyValueValidationState::Partial:
                return GetColor(DebugCommand::DebugCommandOutputKind::Hint);
            case DebugCommand::PropertyValueValidationState::Invalid:
                return GetColor(DebugCommand::DebugCommandOutputKind::Error);
            case DebugCommand::PropertyValueValidationState::None:
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
