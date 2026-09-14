#pragma once

#include "DebugCommand/DebugCommandDescriptor.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine::DebugCommand
{
    class DebugCommandRegistry
    {
    public:
        struct StoredCommand
        {
            std::string Id;
            std::string DisplayName;
            std::string Description;
            DebugCommandScope Scope = DebugCommandScope::Both;
            DebugCommandFlags Flags = DebugCommandFlags::None;
            std::vector<DebugCommandArgDescriptor> Args;
            DebugCommandExecuteFn Execute;
        };

        static DebugCommandRegistry& Get();

        void Register(DebugCommandDescriptor descriptor);
        void Clear();

        const StoredCommand* Find(std::string_view commandId) const;
        std::vector<const StoredCommand*> List(std::string_view prefix, DebugCommandScope scopeFilter) const;
        void ForEach(const std::function<void(const StoredCommand&)>& visitor) const;

    private:
        std::vector<StoredCommand> m_Commands;
    };
}
