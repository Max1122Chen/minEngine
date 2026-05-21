#include "MaterialEdGraphNode.h"

namespace minEngine
{
    void MaterialEdGraphNode::SetNodeDef(std::shared_ptr<MaterialGraphNodeDef> nodeDef)
    {
        m_NodeDef = std::move(nodeDef);
        RebuildPins();
    }

    void MaterialEdGraphNode::RebuildPins()
    {
        Pins.clear();
        if (!m_NodeDef)
        {
            return;
        }

        int inputIndex = 0;
        while (MaterialGraphNodeDef::Input* input = m_NodeDef->GetInput(inputIndex))
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
        while (MaterialGraphNodeDef::Output* output = m_NodeDef->GetOutput(outputIndex))
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
}
