#pragma once

#include "Core.h"

#include "imgui.h"

#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogRecord.h"

#include "UI/Widgets/MultiSelectFilterDropdown.h"
#include "UI/CommandConsole/CommandConsolePresenter.h"

#include "UI/EditorWindows/EditorWindow.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemeScope.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTheme.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

namespace minEngine
{
    class ConsoleWindow final : public EditorWindow
    {
    public:
        explicit ConsoleWindow(IEditorContext& context);

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }

        void OnDraw() override;

    private:
        enum class ConsoleTab : uint8_t
        {
            Output = 0,
            Command,
        };

        void DrawOutputTab();
        void DrawCommandTab();
        float GetCommandModeMinWindowHeight() const;

        bool PassFilter(const LogRecord& entry) const;
        bool PassLevelFilter(LogSeverity severity) const;
        static bool ContainsIgnoreCase(const std::string& text, const char* keyword);
        static std::string FormatTimestamp(const LogRecord& entry);
        ImVec4 GetLevelColor(LogSeverity severity) const;

        const std::string m_Id = "console";
        const std::string m_Title = "Console";
        ConsoleTab m_ActiveTab = ConsoleTab::Output;

        // Minimal S00 mapping: Core/App checkboxes; other channels always shown.
        bool m_ShowCore = true;
        bool m_ShowClient = true;
        bool m_ShowTrace = true;
        bool m_ShowDebug = true;
        bool m_ShowInfo = true;
        bool m_ShowWarn = true;
        bool m_ShowError = true;
        bool m_ShowFatal = true;
        bool m_AutoScroll = true;
        bool m_PauseStream = false;
        bool m_LastIsPlaying = false;
        bool m_HasPausedSnapshot = false;
        std::vector<LogRecord> m_PausedEntries;
        char m_SearchText[128] = {};

        CommandConsolePresenter m_CommandPresenter;
        bool m_CommandAutoScroll = true;
        float m_CommandModeMinWindowHeight = 0.0f;
    };
}
