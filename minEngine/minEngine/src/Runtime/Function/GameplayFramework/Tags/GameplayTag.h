#pragma once

#include <cstdint>
#include <string>

namespace minEngine
{
    class GameplayTagManager;

    /// Handle into a GameplayTagManager vocabulary (UE FGameplayTag analogue).
    class GameplayTag
    {
    public:
        GameplayTag() = default;

        bool IsValid() const;
        uint32_t GetIndex() const { return m_Index; }
        const std::string& GetName() const;

        bool Matches(const GameplayTag& query) const;
        bool IsChildOf(const GameplayTag& ancestor) const;

        bool operator==(const GameplayTag& other) const;
        bool operator!=(const GameplayTag& other) const { return !(*this == other); }

    private:
        friend class GameplayTagManager;

        GameplayTag(const GameplayTagManager* manager, uint32_t index);

        const GameplayTagManager* m_Manager = nullptr;
        uint32_t m_Index = UINT32_MAX;
    };
}
