#pragma once

#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include <vector>

namespace minEngine
{
    // Test host so a vector of tags goes through the same serialize + Setter bind path.
    ME_STRUCT()
    struct GameplayTagListProbe
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        GameplayTag Tag;

        ME_PROPERTY()
        std::vector<GameplayTag> Tags;
    };
}

#include "Generated/Reflection/GameplayTagListProbe.gen.h"
