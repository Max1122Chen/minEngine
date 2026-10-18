#pragma once

#include "Core.h"
#include "Core/Reflection/ReflectionAnnotations.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class GameplayTagManager;

    /// Hierarchical tag handle. Disk identity is TagName; runtime compare uses Manager index after bind.
    ME_STRUCT()
    class GameplayTag
    {
        ME_GENERATED_BODY()

    public:
        GameplayTag() = default;

        bool IsValid() const;
        uint32_t GetIndex() const { return m_Index; }
        const std::string& GetName() const { return TagName; }

        bool Matches(const GameplayTag& query) const;
        bool IsChildOf(const GameplayTag& ancestor) const;

        bool operator==(const GameplayTag& other) const;
        bool operator!=(const GameplayTag& other) const { return !(*this == other); }

        /// Reflection / serialization only. Gameplay code should use Manager::Resolve.
        void SetTagName(std::string name);

    private:
        friend class GameplayTagManager;

        GameplayTag(const GameplayTagManager* manager, uint32_t index, std::string tagName);

        void ClearRuntimeBinding();
        bool TryBindToManager();

        ME_PROPERTY(Invisible, meta = (Setter = "SetTagName", Getter = "GetName"))
        std::string TagName;

        uint32_t m_Index = UINT32_MAX;
        const GameplayTagManager* m_Manager = nullptr;
    };
}

#include "Generated/Reflection/GameplayTag.gen.h"
