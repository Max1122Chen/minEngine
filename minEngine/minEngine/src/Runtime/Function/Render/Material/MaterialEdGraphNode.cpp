#include "MaterialEdGraphNode.h"

namespace minEngine
{
    void MaterialEdGraphNode::SetDefinition(std::unique_ptr<MaterialGraphNodeDef> definition)
    {
        m_Definition = std::move(definition);
        CreateInputPins();
    }

    void MaterialEdGraphNode::CreateInputPins()
    {
        Pins.clear();
        if (!m_Definition)
        {
            return;
        }

        int inputIndex = 0;
        while (MaterialGraphNodeDef::Input* input = m_Definition->GetInput(inputIndex))
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
        while (MaterialGraphNodeDef::Output* output = m_Definition->GetOutput(outputIndex))
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
        CreateInputPins();
    }
}
