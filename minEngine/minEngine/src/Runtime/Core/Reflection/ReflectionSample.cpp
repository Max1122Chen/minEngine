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

    bool ReflectionSampleComponent::MixAlign(bool inputFlag, double threshold) const
    {
        return inputFlag && threshold > 0.0;
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

    Math::Vector2 ReflectionSampleComponent::AddVector2(Math::Vector2 lhs, Math::Vector2 rhs) const
    {
        return lhs + rhs;
    }

    Math::Vector3 ReflectionSampleComponent::AddVector3(Math::Vector3 lhs, Math::Vector3 rhs) const
    {
        return lhs + rhs;
    }

    Math::Vector4 ReflectionSampleComponent::AddVector4(Math::Vector4 lhs, Math::Vector4 rhs) const
    {
        return lhs + rhs;
    }

    void ReflectionSampleComponent::ScaleVector2InPlace(Math::Vector2& value, float scale) const
    {
        value *= scale;
    }

    void ReflectionSampleComponent::ScaleVector3InPlace(Math::Vector3& value, float scale) const
    {
        value *= scale;
    }

    void ReflectionSampleComponent::ScaleVector4InPlace(Math::Vector4& value, float scale) const
    {
        value *= scale;
    }

    void ReflectionSampleComponent::PrefixString(const std::string& prefix, std::string& inOutValue) const
    {
        inOutValue = prefix + inOutValue;
    }

    bool ReflectionSampleComponent::IsValidComponentPtr(Component* candidate) const
    {
        return candidate != nullptr && candidate->IsA(Component::StaticClass());
    }

    void ReflectionSampleComponent::StaticResetCounter()
    {
        s_StaticTestCounter = 0;
    }

    int32_t ReflectionSampleComponent::StaticGetCounter()
    {
        return s_StaticTestCounter;
    }

    int32_t ReflectionSampleComponent::StaticAdd(int32_t firstOperand, int32_t secondOperand)
    {
        return firstOperand + secondOperand;
    }

    void ReflectionSampleComponent::StaticAddInPlace(int32_t& value, int32_t delta)
    {
        value += delta;
    }
}
