#pragma once

#include "Core.h"

#include <optional>
#include <string>
#include <vector>

namespace minEngine::Command
{
    class CommandHistory
    {
    public:
        void Push(std::string line);
        void ResetNavigation();

        std::optional<std::string> NavigateUp(const std::string& currentDraft);
        std::optional<std::string> NavigateDown(const std::string& currentDraft);

        const std::vector<std::string>& GetEntries() const { return m_Entries; }

    private:
        std::vector<std::string> m_Entries;
        int m_NavigationIndex = -1;
        std::string m_SavedDraft;
    };
}
