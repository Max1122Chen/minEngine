#include "Runtime/Core/Command/CommandRegistry.h"

#include <algorithm>

namespace minEngine::Command
{
    CommandRegistry& CommandRegistry::Get()
    {
        static CommandRegistry instance;
        return instance;
    }

    void CommandRegistry::Register(CommandDescriptor descriptor)
    {
        if (descriptor.Id.empty() || !descriptor.Execute)
        {
            return;
        }

        for (const StoredCommand& existing : m_Commands)
        {
            if (existing.Id == descriptor.Id)
            {
                return;
            }
        }

        StoredCommand stored;
        stored.Id = std::string(descriptor.Id);
        stored.DisplayName = std::string(descriptor.DisplayName.empty() ? descriptor.Id : descriptor.DisplayName);
        stored.Description = std::string(descriptor.Description);
        stored.Scope = descriptor.Scope;
        stored.Flags = descriptor.Flags;
        stored.Args = descriptor.Args;
        stored.Execute = std::move(descriptor.Execute);
        m_Commands.push_back(std::move(stored));
    }

    void CommandRegistry::Clear()
    {
        m_Commands.clear();
    }

    const CommandRegistry::StoredCommand* CommandRegistry::Find(std::string_view commandId) const
    {
        for (const StoredCommand& stored : m_Commands)
        {
            if (stored.Id == commandId)
            {
                return &stored;
            }
        }

        return nullptr;
    }

    std::vector<const CommandRegistry::StoredCommand*> CommandRegistry::List(
        std::string_view prefix,
        CommandScope scopeFilter) const
    {
        std::vector<const StoredCommand*> matches;
        matches.reserve(m_Commands.size());

        for (const StoredCommand& stored : m_Commands)
        {
            if (scopeFilter != CommandScope::Both && stored.Scope != CommandScope::Both && stored.Scope != scopeFilter)
            {
                continue;
            }

            if (HasCommandFlag(stored.Flags, CommandFlags::Hidden))
            {
                continue;
            }

            if (!prefix.empty() && stored.Id.rfind(prefix, 0) != 0)
            {
                continue;
            }

            matches.push_back(&stored);
        }

        std::sort(matches.begin(), matches.end(), [](const StoredCommand* lhs, const StoredCommand* rhs) {
            return lhs->Id < rhs->Id;
        });

        return matches;
    }

    void CommandRegistry::ForEach(const std::function<void(const StoredCommand&)>& visitor) const
    {
        for (const StoredCommand& stored : m_Commands)
        {
            visitor(stored);
        }
    }
}
