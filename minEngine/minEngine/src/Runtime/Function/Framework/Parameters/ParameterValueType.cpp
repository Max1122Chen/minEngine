#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"

namespace minEngine
{
    bool ParameterValueTypeUtil::IsKnown(ParameterValueType type)
    {
        switch (type)
        {
        case ParameterValueType::Bool:
        case ParameterValueType::Int32:
        case ParameterValueType::Float:
            return true;
        default:
            return false;
        }
    }

    uint16_t ParameterValueTypeUtil::SizeOf(ParameterValueType type)
    {
        switch (type)
        {
        case ParameterValueType::Bool:
            return 1;
        case ParameterValueType::Int32:
        case ParameterValueType::Float:
            return 4;
        default:
            return 0;
        }
    }

    uint16_t ParameterValueTypeUtil::AlignOf(ParameterValueType type)
    {
        switch (type)
        {
        case ParameterValueType::Bool:
            return 1;
        case ParameterValueType::Int32:
        case ParameterValueType::Float:
            return 4;
        default:
            return 1;
        }
    }
}
