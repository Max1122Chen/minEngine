#pragma once

#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include <string>

namespace minEngine
{
    struct GameplayEventChannel
    {
        GameplayTag Tag;

        const std::string& GetName() const { return Tag.GetName(); }
    };
}
