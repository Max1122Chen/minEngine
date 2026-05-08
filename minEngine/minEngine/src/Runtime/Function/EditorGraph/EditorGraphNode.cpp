#include "EditorGraphNode.h"

namespace minEngine
{
    EditorGraphNodePin* EditorGraphNode::GetInputPin(int32_t Index)
    {
        for (EditorGraphNodePin& pin : Pins)
        {
            if (pin.Direction == EditorGraphPinDirection::Input && pin.Index == Index)
            {
                return &pin;
            }
        }
        return nullptr;
    }

    EditorGraphNodePin* EditorGraphNode::GetOutputPin(int32_t Index)
    {
        for (EditorGraphNodePin& pin : Pins)
        {
            if (pin.Direction == EditorGraphPinDirection::Output && pin.Index == Index)
            {
                return &pin;
            }
        }
        return nullptr;
    }
}
