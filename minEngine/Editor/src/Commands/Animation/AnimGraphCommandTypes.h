#pragma once

#include "Runtime/Function/Animation/AnimationGraph.h"

#include <cstddef>
#include <string>
#include <vector>

namespace minEngine
{
    struct AnimIndexedTransition
    {
        size_t Index = 0;
        AnimTransition Transition;
    };

    /** Enough to put a deleted state and its edges back. */
    struct AnimStateRemovalSnapshot
    {
        AnimState State;
        size_t StateIndex = 0;
        std::vector<AnimIndexedTransition> Transitions;
        std::vector<AnimIndexedTransition> AnyStateTransitions;
        std::string DefaultStateName;
    };
}
