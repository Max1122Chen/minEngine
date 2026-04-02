#pragma once

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

#include "Runtime/Core/Log/LogConsole.h"

#include "IPanel.h"

namespace minEngine
{
    class ConsolePanel final : public IPanel
    {
    public:
        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw(const PanelContext& context) override
        {
            const bool isPlaying = (context.state != nullptr) ? context.state->isPlaying : false;
            if (m_ClearOnPlay && isPlaying && !m_LastIsPlaying)
            {
                LogConsoleStorage::Clear();
            }
            m_LastIsPlaying = isPlaying;

            ImGui::Begin(m_Title.c_str());
            bool requestCopyVisible = false;

            if (ImGui::Button("Clear"))
            {
                LogConsoleStorage::Clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy"))
            {
                requestCopyVisible = true;
            }
            ImGui::SameLine();
            ImGui::Checkbox("AutoScroll", &m_AutoScroll);
            ImGui::SameLine();
            ImGui::Checkbox("Pause", &m_PauseStream);
            ImGui::SameLine();
            ImGui::Checkbox("ClearOnPlay", &m_ClearOnPlay);

            ImGui::Separator();
            ImGui::Checkbox("Core", &m_ShowCore);
            ImGui::SameLine();
            ImGui::Checkbox("Client", &m_ShowClient);

            ImGui::SameLine();
            ImGui::Text("Level:");
            ImGui::SameLine();
            ImGui::Checkbox("Trace", &m_ShowTrace);
            ImGui::SameLine();
            ImGui::Checkbox("Debug", &m_ShowDebug);
            ImGui::SameLine();
            ImGui::Checkbox("Info", &m_ShowInfo);
            ImGui::SameLine();
            ImGui::Checkbox("Warn", &m_ShowWarn);
            ImGui::SameLine();
            ImGui::Checkbox("Error", &m_ShowError);
            ImGui::SameLine();
            ImGui::Checkbox("Critical", &m_ShowCritical);

            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputTextWithHint("##ConsoleSearch", "Search message...", m_SearchText, sizeof(m_SearchText));
            ImGui::Separator();

            const std::vector<LogConsoleEntry> liveEntries = LogConsoleStorage::Snapshot();
            if (m_PauseStream)
            {
                if (!m_HasPausedSnapshot)
                {
                    m_PausedEntries = liveEntries;
                    m_HasPausedSnapshot = true;
                }
            }
            else if (m_HasPausedSnapshot)
            {
                m_PausedEntries.clear();
                m_HasPausedSnapshot = false;
            }

            const std::vector<LogConsoleEntry>& entries = m_PauseStream ? m_PausedEntries : liveEntries;

            std::string clipboardText;

            ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            const bool wasAtBottom = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);
            for (const LogConsoleEntry& entry : entries)
            {
                if (!PassFilter(entry))
                {
                    continue;
                }

                const char* source = "UNKNOWN";
                if (entry.source == LogSource::Core)
                {
                    source = "CORE";
                }
                else if (entry.source == LogSource::Client)
                {
                    source = "CLIENT";
                }

                const char* level = LogLevel::ToString(entry.level);
                ImGui::TextColored(GetLevelColor(entry.level), "[%s] [%s] [%s] %s", entry.timestamp.c_str(), source, level, entry.message.c_str());

                if (requestCopyVisible)
                {
                    clipboardText += "[";
                    clipboardText += entry.timestamp;
                    clipboardText += "] [";
                    clipboardText += source;
                    clipboardText += "] [";
                    clipboardText += level;
                    clipboardText += "] ";
                    clipboardText += entry.message;
                    clipboardText += "\n";
                }
            }
            if (m_AutoScroll && wasAtBottom)
            {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();

            if (requestCopyVisible)
            {
                ImGui::SetClipboardText(clipboardText.c_str());
            }

            ImGui::End();
        }

        bool PassFilter(const LogConsoleEntry& entry) const
        {
            if (entry.source == LogSource::Core && !m_ShowCore)
            {
                return false;
            }

            if (entry.source == LogSource::Client && !m_ShowClient)
            {
                return false;
            }

            if (!PassLevelFilter(entry.level))
            {
                return false;
            }

            if (m_SearchText[0] == '\0')
            {
                return true;
            }

            return ContainsIgnoreCase(entry.message, m_SearchText);
        }

        bool PassLevelFilter(LogLevel::Level level) const
        {
            switch (level)
            {
                case LogLevel::Level::Trace: return m_ShowTrace;
                case LogLevel::Level::Debug: return m_ShowDebug;
                case LogLevel::Level::Info: return m_ShowInfo;
                case LogLevel::Level::Warn: return m_ShowWarn;
                case LogLevel::Level::Error: return m_ShowError;
                case LogLevel::Level::Critical: return m_ShowCritical;
                default: return true;
            }
        }

        static bool ContainsIgnoreCase(const std::string& text, const char* keyword)
        {
            if (keyword == nullptr || keyword[0] == '\0')
            {
                return true;
            }

            const auto it = std::search(
                text.begin(), text.end(),
                keyword, keyword + std::strlen(keyword),
                [](const char lhs, const char rhs)
                {
                    return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
                });
            return it != text.end();
        }

        static ImVec4 GetLevelColor(LogLevel::Level level)
        {
            switch (level)
            {
                case LogLevel::Level::Trace: return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
                case LogLevel::Level::Debug: return ImVec4(0.20f, 0.85f, 1.00f, 1.0f);
                case LogLevel::Level::Info: return ImVec4(0.35f, 0.90f, 0.35f, 1.0f);
                case LogLevel::Level::Warn: return ImVec4(1.00f, 0.90f, 0.20f, 1.0f);
                case LogLevel::Level::Error: return ImVec4(1.00f, 0.35f, 0.35f, 1.0f);
                case LogLevel::Level::Critical: return ImVec4(1.00f, 0.10f, 0.10f, 1.0f);
                default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }

    private:
        const std::string m_Id = "console";
        const std::string m_Title = "Console";
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
        bool m_ClearOnPlay = false;
        bool m_LastIsPlaying = false;
        bool m_HasPausedSnapshot = false;
        std::vector<LogConsoleEntry> m_PausedEntries;
        char m_SearchText[128] = {};
    };
}
