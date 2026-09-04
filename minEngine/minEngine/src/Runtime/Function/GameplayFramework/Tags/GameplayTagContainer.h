#pragma once

#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class GameplayTagManager;

    class GameplayTagContainer
    {
    public:
        explicit GameplayTagContainer(GameplayTagManager& manager);
        GameplayTagContainer(const GameplayTagContainer& other) = default;
        GameplayTagContainer& operator=(const GameplayTagContainer& other) = default;

        GameplayTagManager& GetManager() const { return *m_Manager; }

        void Add(const GameplayTag& tag, int32_t count = 1);
        bool Remove(const GameplayTag& tag, int32_t count = 1);
        void Clear();

        int32_t GetCount(const GameplayTag& tag) const;
        bool Has(const GameplayTag& query) const;
        bool HasAll(const std::vector<GameplayTag>& queries) const;
        bool HasAny(const std::vector<GameplayTag>& queries) const;

        GameplayTagContainer Clone() const;
        std::vector<GameplayTag> ToArray() const;

    private:
        GameplayTagManager* m_Manager = nullptr;
        std::unordered_map<uint32_t, int32_t> m_CountsByIndex;
    };
}
