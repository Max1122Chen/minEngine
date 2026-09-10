#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"

#include "Runtime/Function/GameplayFramework/Tags/NativeGameplayTags.h"

#include "Core.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace minEngine
{
    GameplayTagManager* GameplayTagManager::s_Instance = nullptr;

    GameplayTagManager::~GameplayTagManager()
    {
        if (s_Instance == this)
        {
            s_Instance = nullptr;
        }
    }

    bool GameplayTagManager::HasInstance()
    {
        return s_Instance != nullptr && s_Instance->m_Initialized;
    }

    GameplayTagManager& GameplayTagManager::Get()
    {
        ME_ASSERT(HasInstance(), "GameplayTagManager is not initialized");
        return *s_Instance;
    }

    void GameplayTagManager::SetInstance(GameplayTagManager* instance)
    {
        s_Instance = instance;
    }

    void GameplayTagManager::Initialize()
    {
        ME_ASSERT(!m_Initialized, "GameplayTagManager already initialized");

        std::vector<std::string> names = NativeGameplayTagRegistrar::CopyRegisteredNames();
        BuildFromNames(names);
        m_Initialized = true;
    }

    void GameplayTagManager::Shutdown()
    {
        m_Nodes.clear();
        m_TagsByIndex.clear();
        m_IndexByName.clear();
        m_Initialized = false;
    }

    GameplayTag GameplayTagManager::Resolve(const std::string& name) const
    {
        const GameplayTag tag = TryResolve(name);
        if (!tag.IsValid())
        {
            ME_CORE_ERROR("Unknown GameplayTag: {}", name);
            ME_ASSERT(false, "Unknown GameplayTag");
        }
        return tag;
    }

    GameplayTag GameplayTagManager::TryResolve(const std::string& name) const
    {
        if (!m_Initialized || !IsValidTagName(name))
        {
            return {};
        }

        const auto it = m_IndexByName.find(name);
        if (it == m_IndexByName.end())
        {
            return {};
        }

        return m_TagsByIndex[it->second];
    }

    GameplayTag GameplayTagManager::GetParent(const GameplayTag& tag) const
    {
        if (!IsValidTag(tag))
        {
            return {};
        }

        const uint32_t parentIndex = m_Nodes[tag.GetIndex()].ParentIndex;
        if (parentIndex == UINT32_MAX)
        {
            return {};
        }

        return m_TagsByIndex[parentIndex];
    }

    bool GameplayTagManager::IsValidTag(const GameplayTag& tag) const
    {
        if (!m_Initialized || tag.m_Manager != this)
        {
            return false;
        }

        return tag.m_Index < m_TagsByIndex.size()
            && m_TagsByIndex[tag.m_Index].m_Index == tag.m_Index;
    }

    const std::string& GameplayTagManager::GetTagName(const GameplayTag& tag) const
    {
        ME_ASSERT(IsValidTag(tag), "Invalid GameplayTag");
        return m_Nodes[tag.GetIndex()].Name;
    }

    void GameplayTagManager::BuildFromNames(const std::vector<std::string>& names)
    {
        std::set<std::string> uniqueNames;
        for (const std::string& name : names)
        {
            ME_ASSERT(IsValidTagName(name), "Invalid GameplayTag name in native registration");
            uniqueNames.insert(name);
            AddImplicitParents(name, uniqueNames);
        }

        std::vector<std::string> sortedNames(uniqueNames.begin(), uniqueNames.end());
        std::sort(sortedNames.begin(), sortedNames.end());

        m_Nodes.clear();
        m_TagsByIndex.clear();
        m_IndexByName.clear();
        m_Nodes.reserve(sortedNames.size());
        m_TagsByIndex.reserve(sortedNames.size());

        for (uint32_t index = 0; index < sortedNames.size(); ++index)
        {
            TagNode node;
            node.Name = sortedNames[index];
            m_IndexByName.emplace(node.Name, index);
            m_Nodes.push_back(std::move(node));
            m_TagsByIndex.push_back(GameplayTag(this, index));
        }

        for (uint32_t index = 0; index < m_Nodes.size(); ++index)
        {
            const std::string parentName = GetParentName(m_Nodes[index].Name);
            if (parentName.empty())
            {
                m_Nodes[index].ParentIndex = UINT32_MAX;
                continue;
            }

            const auto parentIt = m_IndexByName.find(parentName);
            ME_ASSERT(parentIt != m_IndexByName.end(), "Missing parent GameplayTag node");
            m_Nodes[index].ParentIndex = parentIt->second;
        }
    }

    bool GameplayTagManager::IsValidTagName(const std::string& name)
    {
        if (name.empty())
        {
            return false;
        }

        bool expectSegmentStart = true;
        for (const char ch : name)
        {
            if (expectSegmentStart)
            {
                if (!std::isalnum(static_cast<unsigned char>(ch)))
                {
                    return false;
                }
                expectSegmentStart = false;
                continue;
            }

            if (ch == '.')
            {
                expectSegmentStart = true;
                continue;
            }

            if (!std::isalnum(static_cast<unsigned char>(ch)))
            {
                return false;
            }
        }

        return !expectSegmentStart;
    }

    std::string GameplayTagManager::GetParentName(const std::string& name)
    {
        const std::size_t lastDot = name.find_last_of('.');
        if (lastDot == std::string::npos)
        {
            return {};
        }
        return name.substr(0, lastDot);
    }

    void GameplayTagManager::AddImplicitParents(const std::string& name, std::set<std::string>& inoutNames)
    {
        std::string current = GetParentName(name);
        while (!current.empty())
        {
            inoutNames.insert(current);
            current = GetParentName(current);
        }
    }
}
