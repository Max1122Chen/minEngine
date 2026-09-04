#pragma once

#include "Runtime/Function/GameplayFramework/Tags/GameplayTagContainer.h"

namespace minEngine
{
    /// Tag-described gameplay message. Payload is deferred (GP-F02).
    struct GameplayEvent
    {
        GameplayTagContainer Tags;

        explicit GameplayEvent(GameplayTagManager& manager)
            : Tags(manager)
        {
        }
    };
}
