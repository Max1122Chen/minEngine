#include "MaterialGraphNodeDef.h"

namespace minEngine
{
    MaterialGraphNodeDefOutput* MaterialGraphNodeDefInput::GetConnectedOutput() const
    {
        return IsConnected() ? NodeDef->GetOutput(OutputIndex) : nullptr;
    }

    MaterialGraphNodeDef::Input* MaterialGraphNodeDef::FindInputByName(const char* name)
    {
        if (name == nullptr)
        {
            return nullptr;
        }

        for (Input& input : m_Inputs)
        {
            if (input.Name == name)
            {
                return &input;
            }
        }

        return nullptr;
    }

    const MaterialGraphNodeDef::Input* MaterialGraphNodeDef::FindInputByName(const char* name) const
    {
        return const_cast<MaterialGraphNodeDef*>(this)->FindInputByName(name);
    }

    MaterialValueType MaterialGraphNodeDef::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex < 0 || outputIndex >= GetOutputCount())
        {
            return nullptr;
        }
        return nullptr;
    }

    MaterialValueType MaterialGraphNodeDef::GetInputPinType(int32_t inputIndex) const
    {
        if (inputIndex < 0 || inputIndex >= GetInputCount())
        {
            return nullptr;
        }
        return nullptr;
    }
}

