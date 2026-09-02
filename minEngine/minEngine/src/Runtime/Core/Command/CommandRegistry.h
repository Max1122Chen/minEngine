#pragma once

#include "Runtime/Core/Command/CommandDescriptor.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine::Command
{
    class CommandRegistry
    {
    public:
        struct StoredCommand
        {
            std::string Id;
            std::string DisplayName;
            std::string Description;
            CommandScope Scope = CommandScope::Both;
            CommandFlags Flags = CommandFlags::None;
            std::vector<CommandArgDescriptor> Args;
            CommandExecuteFn Execute;
        };

        static CommandRegistry& Get();

        void Register(CommandDescriptor descriptor);
        void Clear();

        const StoredCommand* Find(std::string_view commandId) const;
        std::vector<const StoredCommand*> List(std::string_view prefix, CommandScope scopeFilter) const;
        void ForEach(const std::function<void(const StoredCommand&)>& visitor) const;

    private:
        std::vector<StoredCommand> m_Commands;
    };
}
