#pragma once

#include "Core.h"

#include "imgui.h"

#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogRecord.h"
#include "Runtime/Core/Log/LogSystem.h"

#include "UI/Widgets/MultiSelectFilterDropdown.h"
#include "UI/CommandConsole/CommandConsolePresenter.h"

#include "UI/EditorWindows/EditorWindow.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemeScope.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTheme.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

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

        enum class TimestampDisplayMode : uint8_t
        {
            Off = 0,
            Time,
            DateTime,
        };

        enum class TimeWindowMode : uint8_t
        {
            Off = 0,
            LastSeconds,
            SincePlay,
            SinceClear,
        };

        struct CollapsedRow
        {
            const LogRecord* record = nullptr;
            int count = 1;
        };

        void DrawOutputTab();
        void DrawCommandTab();
        float GetCommandModeMinWindowHeight() const;

        void EnsureChannelFilterState();
        void HandleClearOnPlayTransition(bool isPlaying);
        bool PassFilter(const LogRecord& entry, const std::chrono::system_clock::time_point& now) const;
        bool PassLevelFilter(LogSeverity severity) const;
        bool PassTimeFilter(const LogRecord& entry, const std::chrono::system_clock::time_point& now) const;
        static bool ContainsIgnoreCase(const std::string& text, const char* keyword);
        std::string FormatTimestamp(const LogRecord& entry) const;
        ImVec4 GetLevelColor(LogSeverity severity) const;
        void BuildCollapsedRows(const std::vector<LogRecord>& entries,
                                const std::chrono::system_clock::time_point& now,
                                std::vector<CollapsedRow>& outRows) const;

        const std::string m_Id = "console";
        const std::string m_Title = "Console";
        ConsoleTab m_ActiveTab = ConsoleTab::Output;

        std::unordered_map<std::string, bool> m_ChannelEnabled;
        std::vector<std::string> m_ChannelOrder;

        bool m_ShowTrace = true;
        bool m_ShowDebug = true;
        bool m_ShowInfo = true;
        bool m_ShowWarn = true;
        bool m_ShowError = true;
        bool m_ShowFatal = true;
        bool m_AutoScroll = true;
        bool m_PauseStream = false;
        bool m_CollapseDuplicates = false;
        bool m_ClearOnPlay = true;
        bool m_WasPlaying = false;
        bool m_HasPausedSnapshot = false;
        std::vector<LogRecord> m_PausedEntries;
        std::vector<LogRecord> m_LiveCache;
        uint64_t m_LiveCacheGeneration = static_cast<uint64_t>(-1);
        char m_SearchText[128] = {};

        TimestampDisplayMode m_TimestampDisplay = TimestampDisplayMode::Time;
        TimeWindowMode m_TimeWindowMode = TimeWindowMode::Off;
        float m_TimeWindowLastSeconds = 10.0f;
        std::chrono::system_clock::time_point m_PlayStartedAt{};
        bool m_HasPlayStartedAt = false;
        std::chrono::system_clock::time_point m_ClearedAt = std::chrono::system_clock::now();

        CommandConsolePresenter m_CommandPresenter;
        bool m_CommandAutoScroll = true;
        float m_CommandModeMinWindowHeight = 0.0f;
    };
}
