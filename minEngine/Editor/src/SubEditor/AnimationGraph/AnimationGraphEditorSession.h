#pragma once

#include "Core.h"

#include "Runtime/Function/Animation/AnimationGraph.h"

#include <memory>
#include <string>

namespace minEngine
{
    enum class AnimGraphSelectionKind : uint8_t
    {
        None = 0,
        State = 1,
        Transition = 2,
        AnyStateTransition = 3,
    };

    struct AnimGraphSelection
    {
        AnimGraphSelectionKind Kind = AnimGraphSelectionKind::None;
        std::string StateName;
        int TransitionIndex = -1;

        void Clear()
        {
            Kind = AnimGraphSelectionKind::None;
            StateName.clear();
            TransitionIndex = -1;
        }

        bool HasSelection() const { return Kind != AnimGraphSelectionKind::None; }
    };

    struct AnimationGraphEditorSession
    {
        std::shared_ptr<AnimationGraph> GraphAsset;
        std::string AssetPath;
        bool Dirty = false;
        AnimGraphSelection Selection;

        bool HasOpenGraph() const
        {
            return GraphAsset != nullptr && !AssetPath.empty();
        }

        void Clear()
        {
            GraphAsset.reset();
            AssetPath.clear();
            Dirty = false;
            Selection.Clear();
        }
    };
}
