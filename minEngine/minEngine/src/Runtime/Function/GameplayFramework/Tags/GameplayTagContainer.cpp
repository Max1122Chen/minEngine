#include "Runtime/Function/GameplayFramework/Tags/GameplayTagContainer.h"

#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"

#include "Core.h"

namespace minEngine
{
    GameplayTagContainer::GameplayTagContainer(GameplayTagManager& manager)
        : m_Manager(&manager)
    {
    }

    void GameplayTagContainer::Add(const GameplayTag& tag, int32_t count)
    {
        ME_ASSERT(m_Manager != nullptr, "GameplayTagContainer has no manager");
        ME_ASSERT(count > 0, "GameplayTagContainer::Add count must be positive");
        ME_ASSERT(m_Manager->IsValidTag(tag), "GameplayTagContainer::Add invalid tag");

        m_CountsByIndex[tag.GetIndex()] += count;
    }

    bool GameplayTagContainer::Remove(const GameplayTag& tag, int32_t count)
    {
        ME_ASSERT(m_Manager != nullptr, "GameplayTagContainer has no manager");
        ME_ASSERT(count > 0, "GameplayTagContainer::Remove count must be positive");
        if (!m_Manager->IsValidTag(tag))
        {
            return false;
        }

        const auto it = m_CountsByIndex.find(tag.GetIndex());
        if (it == m_CountsByIndex.end())
        {
            return false;
        }

        it->second -= count;
        if (it->second <= 0)
        {
            m_CountsByIndex.erase(it);
        }
        return true;
    }

    void GameplayTagContainer::Clear()
    {
        m_CountsByIndex.clear();
    }

    int32_t GameplayTagContainer::GetCount(const GameplayTag& tag) const
    {
        if (!m_Manager || !m_Manager->IsValidTag(tag))
        {
            return 0;
        }

        const auto it = m_CountsByIndex.find(tag.GetIndex());
        return it == m_CountsByIndex.end() ? 0 : it->second;
    }

    bool GameplayTagContainer::Has(const GameplayTag& query) const
    {
        if (!m_Manager || !m_Manager->IsValidTag(query))
        {
            return false;
        }

        for (const auto& [index, count] : m_CountsByIndex)
        {
            if (count <= 0)
            {
                continue;
            }

            const GameplayTag owned = m_Manager->ListTags()[index];
            if (owned.Matches(query))
            {
                return true;
            }
        }

        return false;
    }

    bool GameplayTagContainer::HasAll(const std::vector<GameplayTag>& queries) const
    {
        for (const GameplayTag& query : queries)
        {
            if (!Has(query))
            {
                return false;
            }
        }
        return true;
    }

    bool GameplayTagContainer::HasAny(const std::vector<GameplayTag>& queries) const
    {
        for (const GameplayTag& query : queries)
        {
            if (Has(query))
            {
                return true;
            }
        }
        return false;
    }

    GameplayTagContainer GameplayTagContainer::Clone() const
    {
        return *this;
    }

    std::vector<GameplayTag> GameplayTagContainer::ToArray() const
    {
        std::vector<GameplayTag> result;
        result.reserve(m_CountsByIndex.size());
        for (const auto& [index, count] : m_CountsByIndex)
        {
            if (count > 0)
            {
                result.push_back(m_Manager->ListTags()[index]);
            }
        }
        return result;
    }
}
