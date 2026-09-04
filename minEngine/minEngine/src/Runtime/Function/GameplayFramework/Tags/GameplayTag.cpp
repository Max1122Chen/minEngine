#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"

#include "Core.h"

namespace minEngine
{
    GameplayTag::GameplayTag(const GameplayTagManager* manager, uint32_t index)
        : m_Manager(manager)
        , m_Index(index)
    {
    }

    bool GameplayTag::IsValid() const
    {
        return m_Manager != nullptr && m_Manager->IsValidTag(*this);
    }

    const std::string& GameplayTag::GetName() const
    {
        static const std::string s_EmptyTagName;
        if (!IsValid())
        {
            return s_EmptyTagName;
        }

        return m_Manager->GetTagName(*this);
    }

    bool GameplayTag::Matches(const GameplayTag& query) const
    {
        if (!IsValid() || !query.IsValid())
        {
            return false;
        }

        if (m_Index == query.m_Index && m_Manager == query.m_Manager)
        {
            return true;
        }

        return IsChildOf(query);
    }

    bool GameplayTag::IsChildOf(const GameplayTag& ancestor) const
    {
        if (!IsValid() || !ancestor.IsValid() || m_Manager != ancestor.m_Manager)
        {
            return false;
        }

        if (m_Index == ancestor.m_Index)
        {
            return false;
        }

        GameplayTag current = m_Manager->GetParent(*this);
        while (current.IsValid())
        {
            if (current.m_Index == ancestor.m_Index)
            {
                return true;
            }
            current = m_Manager->GetParent(current);
        }

        return false;
    }

    bool GameplayTag::operator==(const GameplayTag& other) const
    {
        return m_Manager == other.m_Manager && m_Index == other.m_Index;
    }
}
