#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"

#include "Core.h"

namespace minEngine
{
    GameplayTag::GameplayTag(const GameplayTagManager* manager, uint32_t index, std::string tagName)
        : TagName(std::move(tagName))
        , m_Index(index)
        , m_Manager(manager)
    {
    }

    void GameplayTag::ClearRuntimeBinding()
    {
        m_Index = UINT32_MAX;
        m_Manager = nullptr;
    }

    bool GameplayTag::TryBindToManager()
    {
        if (TagName.empty())
        {
            ClearRuntimeBinding();
            return false;
        }

        if (!GameplayTagManager::HasInstance())
        {
            ClearRuntimeBinding();
            return false;
        }

        return GameplayTagManager::Get().BindTag(*this);
    }

    void GameplayTag::SetTagName(std::string name)
    {
        TagName = std::move(name);
        TryBindToManager();
    }

    bool GameplayTag::IsValid() const
    {
        return m_Manager != nullptr && m_Manager->IsValidTag(*this);
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
        const bool selfValid = IsValid();
        const bool otherValid = other.IsValid();
        if (selfValid && otherValid)
        {
            return m_Manager == other.m_Manager && m_Index == other.m_Index;
        }

        return !selfValid && !otherValid && TagName.empty() && other.TagName.empty();
    }
}
