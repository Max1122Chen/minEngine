#include "DebugCommand/DebugCommandRegistry.h"

#include <algorithm>

namespace minEngine::DebugCommand
{
    DebugCommandRegistry& DebugCommandRegistry::Get()
    {
        static DebugCommandRegistry instance;
        return instance;
    }

    void DebugCommandRegistry::Register(DebugCommandDescriptor descriptor)
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

    void DebugCommandRegistry::Clear()
    {
        m_Commands.clear();
    }

    const DebugCommandRegistry::StoredCommand* DebugCommandRegistry::Find(std::string_view commandId) const
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

    std::vector<const DebugCommandRegistry::StoredCommand*> DebugCommandRegistry::List(
        std::string_view prefix,
        DebugCommandScope scopeFilter) const
    {
        std::vector<const StoredCommand*> matches;
        matches.reserve(m_Commands.size());

        for (const StoredCommand& stored : m_Commands)
        {
            if (scopeFilter != DebugCommandScope::Both && stored.Scope != DebugCommandScope::Both && stored.Scope != scopeFilter)
            {
                continue;
            }

            if (HasDebugCommandFlag(stored.Flags, DebugCommandFlags::Hidden))
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

    void DebugCommandRegistry::ForEach(const std::function<void(const StoredCommand&)>& visitor) const
    {
        for (const StoredCommand& stored : m_Commands)
        {
            visitor(stored);
        }
    }
}
