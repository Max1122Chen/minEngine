#include "MaterialEdGraphNode.h"

namespace minEngine
{
    void MaterialEdGraphNode::CreateInputPins()
    {
        // Rebuild all pins (inputs + outputs) so the node always has consistent pin list regardless of call order.
        Pins.clear();
        if (!m_Definition) return;

        int inputIndex = 0;
        while (auto input = m_Definition->GetInput(inputIndex))
        {
            EditorGraphNodePin pin;
            pin.Name = input->Name;
            pin.Direction = EditorGraphPinDirection::Input;
            pin.Index = inputIndex;
            pin.SetOwner(this);
            Pins.push_back(pin);
            ++inputIndex;
        }

        int outputIndex = 0;
        while (auto output = m_Definition->GetOutput(outputIndex))
        {
            EditorGraphNodePin pin;
            pin.Name = output->Name;
            pin.Direction = EditorGraphPinDirection::Output;
            pin.Index = outputIndex;
            pin.SetOwner(this);
            Pins.push_back(pin);
            ++outputIndex;
        }
    }

    void MaterialEdGraphNode::CreateOutputPins()
    {
        // Delegate to the same rebuild logic as CreateInputPins to maintain consistency.
        CreateInputPins();
    }
}