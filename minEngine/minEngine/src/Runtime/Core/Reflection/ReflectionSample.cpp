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

    void ReflectionSampleComponent::AddInPlace(int32_t& value, int32_t delta)
    {
        value += delta;
    }

    void ReflectionSampleComponent::PeekString(const std::string& input, int32_t& outLength) const
    {
        outLength = static_cast<int32_t>(input.size());
    }

    void ReflectionSampleComponent::FillOut(int32_t value, int32_t& outValue)
    {
        outValue = value;
    }

    ReflectionSampleEnum ReflectionSampleComponent::EchoEnum(ReflectionSampleEnum value) const
    {
        return value;
    }

    int32_t ReflectionSampleComponent::SumIntArray(const std::vector<int>& values) const
    {
        return static_cast<int32_t>(values.size());
    }

    bool ReflectionSampleComponent::IsSameObject(MEObject* a, MEObject* b) const
    {
        return a == b;
    }
}
