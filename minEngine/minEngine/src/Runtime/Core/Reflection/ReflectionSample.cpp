#include "ReflectionSample.h"

namespace minEngine
{
    void ReflectionSampleComponent::ResetCounter()
    {
        m_FunctionTestCounter = 0;
    }

    int32_t ReflectionSampleComponent::GetCounter() const
    {
        return m_FunctionTestCounter;
    }

    int32_t ReflectionSampleComponent::Add(int32_t firstOperand, int32_t secondOperand)
    {
        return firstOperand + secondOperand;
    }
}
