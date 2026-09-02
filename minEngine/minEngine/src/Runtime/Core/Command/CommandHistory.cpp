#include "Runtime/Core/Command/CommandHistory.h"

namespace minEngine::Command
{
    void CommandHistory::Push(std::string line)
    {
        if (line.empty())
        {
            return;
        }

        if (!m_Entries.empty() && m_Entries.back() == line)
        {
            ResetNavigation();
            return;
        }

        m_Entries.push_back(std::move(line));
        ResetNavigation();
    }

    void CommandHistory::ResetNavigation()
    {
        m_NavigationIndex = -1;
        m_SavedDraft.clear();
    }

    std::optional<std::string> CommandHistory::NavigateUp(const std::string& currentDraft)
    {
        if (m_Entries.empty())
        {
            return std::nullopt;
        }

        if (m_NavigationIndex < 0)
        {
            m_SavedDraft = currentDraft;
            m_NavigationIndex = static_cast<int>(m_Entries.size());
        }

        if (m_NavigationIndex <= 0)
        {
            return std::nullopt;
        }

        --m_NavigationIndex;
        return m_Entries[static_cast<size_t>(m_NavigationIndex)];
    }

    std::optional<std::string> CommandHistory::NavigateDown(const std::string& currentDraft)
    {
        (void)currentDraft;
        if (m_Entries.empty() || m_NavigationIndex < 0)
        {
            return std::nullopt;
        }

        if (m_NavigationIndex + 1 >= static_cast<int>(m_Entries.size()))
        {
            m_NavigationIndex = -1;
            if (m_SavedDraft.empty())
            {
                return std::string{};
            }

            const std::string restoredDraft = m_SavedDraft;
            m_SavedDraft.clear();
            return restoredDraft;
        }

        ++m_NavigationIndex;
        return m_Entries[static_cast<size_t>(m_NavigationIndex)];
    }
}
