#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"

#include "Core.h"

namespace minEngine
{
    GameplayTag::GameplayTag(uint32_t index, std::string tagName)
        : TagName(std::move(tagName))
        , m_Index(index)
    {
    }

    void GameplayTag::ClearRuntimeBinding()
    {
        m_Index = UINT32_MAX;
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
        if (!GameplayTagManager::HasInstance())
        {
            return false;
        }

        return GameplayTagManager::Get().IsValidTag(*this);
    }

    bool GameplayTag::Matches(const GameplayTag& query) const
    {
        if (!IsValid() || !query.IsValid())
        {
            return false;
        }

        if (m_Index == query.m_Index)
        {
            return true;
        }

        return IsChildOf(query);
    }

    bool GameplayTag::IsChildOf(const GameplayTag& ancestor) const
    {
        if (!IsValid() || !ancestor.IsValid())
        {
            return false;
        }

        if (m_Index == ancestor.m_Index)
        {
            return false;
        }

        GameplayTagManager& manager = GameplayTagManager::Get();
        GameplayTag current = manager.GetParent(*this);
        while (current.IsValid())
        {
            if (current.m_Index == ancestor.m_Index)
            {
                return true;
            }
            current = manager.GetParent(current);
        }

        return false;
    }

}
