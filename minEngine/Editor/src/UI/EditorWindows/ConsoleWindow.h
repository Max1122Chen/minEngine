#pragma once

#include "Core.h"

#include "imgui.h"

#include "Runtime/Core/Log/LogConsole.h"

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

        bool PassFilter(const LogConsoleEntry& entry) const;
        bool PassLevelFilter(LogLevel::Level level) const;
        static bool ContainsIgnoreCase(const std::string& text, const char* keyword);
        ImVec4 GetLevelColor(LogLevel::Level level) const;

        const std::string m_Id = "console";
        const std::string m_Title = "Console";
        ConsoleTab m_ActiveTab = ConsoleTab::Output;

        bool m_ShowCore = true;
        bool m_ShowClient = true;
        bool m_ShowTrace = true;
        bool m_ShowDebug = true;
        bool m_ShowInfo = true;
        bool m_ShowWarn = true;
        bool m_ShowError = true;
        bool m_ShowCritical = true;
        bool m_AutoScroll = true;
        bool m_PauseStream = false;
        bool m_LastIsPlaying = false;
        bool m_HasPausedSnapshot = false;
        std::vector<LogConsoleEntry> m_PausedEntries;
        char m_SearchText[128] = {};

        CommandConsolePresenter m_CommandPresenter;
        bool m_CommandAutoScroll = true;
        float m_CommandModeMinWindowHeight = 0.0f;
    };
}
