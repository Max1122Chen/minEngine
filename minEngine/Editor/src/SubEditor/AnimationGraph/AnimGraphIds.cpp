#include "AnimGraphIds.h"

namespace minEngine
{
    namespace Ed = ax::NodeEditor;

    Ed::NodeId AnimGraphIds::ToStateNodeId(size_t stateIndex)
    {
        return Ed::NodeId(static_cast<int64_t>(stateIndex) + 1);
    }

    bool AnimGraphIds::FromStateNodeId(Ed::NodeId id, size_t& outStateIndex)
    {
        const int64_t value = id.Get();
        if (value <= 0 || value >= kTransitionLinkBase)
        {
            return false;
        }
        outStateIndex = static_cast<size_t>(value - 1);
        return true;
    }

    Ed::PinId AnimGraphIds::ToStateInputPinId(size_t stateIndex)
    {
        return Ed::PinId((static_cast<int64_t>(stateIndex) + 1) * 10 + 1);
    }

    Ed::PinId AnimGraphIds::ToStateOutputPinId(size_t stateIndex)
    {
        return Ed::PinId((static_cast<int64_t>(stateIndex) + 1) * 10 + 2);
    }

    bool AnimGraphIds::FromStatePinId(Ed::PinId id, size_t& outStateIndex, Ed::PinKind& outKind)
    {
        const int64_t value = id.Get();
        if (value < 10)
        {
            return false;
        }

        const int64_t nodeOrdinal = value / 10;
        const int64_t pinKindCode = value % 10;
        if (nodeOrdinal <= 0 || nodeOrdinal >= kTransitionLinkBase)
        {
            return false;
        }

        if (pinKindCode == 1)
        {
            outKind = Ed::PinKind::Input;
        }
        else if (pinKindCode == 2)
        {
            outKind = Ed::PinKind::Output;
        }
        else
        {
            return false;
        }

        outStateIndex = static_cast<size_t>(nodeOrdinal - 1);
        return true;
    }

    Ed::LinkId AnimGraphIds::ToTransitionLinkId(size_t transitionIndex)
    {
        return Ed::LinkId(kTransitionLinkBase + static_cast<int64_t>(transitionIndex));
    }

    bool AnimGraphIds::FromTransitionLinkId(Ed::LinkId id, size_t& outTransitionIndex)
    {
        const int64_t value = id.Get();
        if (value < kTransitionLinkBase || value >= kAnyStateLinkBase)
        {
            return false;
        }
        outTransitionIndex = static_cast<size_t>(value - kTransitionLinkBase);
        return true;
    }

    Ed::LinkId AnimGraphIds::ToAnyStateLinkId(size_t anyTransitionIndex)
    {
        return Ed::LinkId(kAnyStateLinkBase + static_cast<int64_t>(anyTransitionIndex));
    }

    bool AnimGraphIds::FromAnyStateLinkId(Ed::LinkId id, size_t& outAnyTransitionIndex)
    {
        const int64_t value = id.Get();
        if (value < kAnyStateLinkBase)
        {
            return false;
        }
        outAnyTransitionIndex = static_cast<size_t>(value - kAnyStateLinkBase);
        return true;
    }
}
