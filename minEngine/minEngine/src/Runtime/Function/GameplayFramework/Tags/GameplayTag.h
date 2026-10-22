#pragma once

#include "Core.h"
#include "Core/Reflection/ReflectionAnnotations.h"

#include <cstdint>
#include <functional>
#include <string>

namespace minEngine
{
    class GameplayTagManager;

    /// Hierarchical tag handle. Identity is TagName; Manager index is an optional bind cache.
    ME_STRUCT()
    class MINENGINE_API GameplayTag
    {
        ME_GENERATED_BODY()

    public:
        GameplayTag() = default;

        bool IsValid() const;
        uint32_t GetIndex() const { return m_Index; }
        const std::string& GetName() const { return TagName; }

        bool Matches(const GameplayTag& query) const;
        bool IsChildOf(const GameplayTag& ancestor) const;

        bool operator==(const GameplayTag& other) const { return TagName == other.TagName; }
        bool operator!=(const GameplayTag& other) const { return !(*this == other); }
        bool operator<(const GameplayTag& other) const { return TagName < other.TagName; }

        /// Reflection / serialization only. Gameplay code should use Manager::Resolve.
        void SetTagName(std::string name);

    private:
        friend class GameplayTagManager;

        GameplayTag(uint32_t index, std::string tagName);

        void ClearRuntimeBinding();
        bool TryBindToManager();

        ME_PROPERTY(Invisible, meta = (Setter = "SetTagName", Getter = "GetName"))
        std::string TagName;

        uint32_t m_Index = UINT32_MAX;
    };
}

namespace std
{
    template <>
    struct hash<minEngine::GameplayTag>
    {
        size_t operator()(const minEngine::GameplayTag& tag) const noexcept
        {
            return hash<string>()(tag.GetName());
        }
    };
}

#include "Generated/Reflection/GameplayTag.gen.h"
