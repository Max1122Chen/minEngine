#pragma once

#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class GameplayTagManager
    {
    public:
        GameplayTagManager() = default;
        ~GameplayTagManager();

        GameplayTagManager(const GameplayTagManager&) = delete;
        GameplayTagManager& operator=(const GameplayTagManager&) = delete;

        void Initialize();
        void Shutdown();

        static bool HasInstance();
        static GameplayTagManager& Get();
        static void SetInstance(GameplayTagManager* instance);

        GameplayTag Resolve(const std::string& name) const;
        GameplayTag TryResolve(const std::string& name) const;

        GameplayTag GetParent(const GameplayTag& tag) const;
        bool IsValidTag(const GameplayTag& tag) const;
        const std::string& GetTagName(const GameplayTag& tag) const;

        const std::vector<GameplayTag>& ListTags() const { return m_TagsByIndex; }

    private:
        friend class GameplayTag;

        struct TagNode
        {
            std::string Name;
            uint32_t ParentIndex = UINT32_MAX;
        };

        void BuildFromNames(const std::vector<std::string>& names);
        static bool IsValidTagName(const std::string& name);
        static std::string GetParentName(const std::string& name);
        static void AddImplicitParents(const std::string& name, std::set<std::string>& inoutNames);

        static GameplayTagManager* s_Instance;

        bool m_Initialized = false;
        std::vector<TagNode> m_Nodes;
        std::vector<GameplayTag> m_TagsByIndex;
        std::unordered_map<std::string, uint32_t> m_IndexByName;
    };
}
