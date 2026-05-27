#pragma once

#include "Core.h"

#include <memory>
#include <typeindex>
#include <vector>

namespace minEngine
{
    class EditorMenuContext
    {
    public:
        template<typename TContext>
        void Add(std::shared_ptr<TContext> ctx)
        {
            if (!ctx)
            {
                return;
            }

            m_Entries.push_back(Entry{
                std::type_index(typeid(TContext)),
                std::static_pointer_cast<void>(ctx)});
        }

        template<typename TContext>
        TContext* Find()
        {
            const std::type_index targetType = std::type_index(typeid(TContext));
            for (Entry& entry : m_Entries)
            {
                if (entry.Type == targetType)
                {
                    return static_cast<TContext*>(entry.Object.get());
                }
            }
            return nullptr;
        }

        template<typename TContext>
        const TContext* Find() const
        {
            const std::type_index targetType = std::type_index(typeid(TContext));
            for (const Entry& entry : m_Entries)
            {
                if (entry.Type == targetType)
                {
                    return static_cast<const TContext*>(entry.Object.get());
                }
            }
            return nullptr;
        }

        template<typename TContext>
        std::vector<TContext*> FindAll()
        {
            std::vector<TContext*> result;
            const std::type_index targetType = std::type_index(typeid(TContext));
            for (Entry& entry : m_Entries)
            {
                if (entry.Type == targetType)
                {
                    result.push_back(static_cast<TContext*>(entry.Object.get()));
                }
            }
            return result;
        }

        template<typename TContext>
        std::vector<const TContext*> FindAll() const
        {
            std::vector<const TContext*> result;
            const std::type_index targetType = std::type_index(typeid(TContext));
            for (const Entry& entry : m_Entries)
            {
                if (entry.Type == targetType)
                {
                    result.push_back(static_cast<const TContext*>(entry.Object.get()));
                }
            }
            return result;
        }

        void Clear();

    private:
        struct Entry
        {
            std::type_index Type;
            std::shared_ptr<void> Object;
        };

        std::vector<Entry> m_Entries;
    };

} // namespace minEngine
