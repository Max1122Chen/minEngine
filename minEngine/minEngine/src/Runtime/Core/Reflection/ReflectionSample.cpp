#include "ReflectionSample.h"
#include "Reflection.h"

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
        int32_t sum = 0;
        for (int element : values)
        {
            sum += static_cast<int32_t>(element);
        }
        return sum;
    }

    bool ReflectionSampleComponent::IsSameObject(MEObject* a, MEObject* b) const
    {
        return a == b;
    }

    std::string ReflectionSampleComponent::GetGreeting(const std::string& name) const
    {
        return "Hello, " + name;
    }

    std::shared_ptr<Component> ReflectionSampleComponent::MakeSharedComponent(bool returnNull) const
    {
        if (returnNull)
        {
            return nullptr;
        }

        const Reflection::ReflectionSystem& reflection = Reflection::ReflectionSystem::Get();
        const Reflection::MEClass* componentClass = reflection.FindClass("minEngine::Component");
        if (componentClass == nullptr)
        {
            return nullptr;
        }

        return std::static_pointer_cast<Component>(componentClass->CreateDefaultInstance());
    }

    bool ReflectionSampleComponent::RewriteSharedComponentRef(std::shared_ptr<Component>& inOutCandidate,
                                                              bool assignNull) const
    {
        if (assignNull)
        {
            inOutCandidate.reset();
            return true;
        }

        const Reflection::ReflectionSystem& reflection = Reflection::ReflectionSystem::Get();
        const Reflection::MEClass* componentClass = reflection.FindClass("minEngine::Component");
        if (componentClass == nullptr)
        {
            return false;
        }

        inOutCandidate = std::static_pointer_cast<Component>(componentClass->CreateDefaultInstance());
        return static_cast<bool>(inOutCandidate);
    }

    std::vector<std::vector<int>> ReflectionSampleComponent::NormalizeNested(
        std::vector<std::vector<int>> values) const
    {
        std::vector<std::vector<int>> output = values;
        for (std::vector<int>& row : output)
        {
            for (int& value : row)
            {
                value *= 2;
            }
        }
        return output;
    }

    int32_t ReflectionSampleComponent::s_StaticTestCounter = 0;

    void ReflectionSampleComponent::SetStaticTestCounter(int32_t value)
    {
        s_StaticTestCounter = value;
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
