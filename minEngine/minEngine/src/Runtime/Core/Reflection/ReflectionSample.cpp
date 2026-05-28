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

    void ReflectionSampleComponent::AppendInt(std::vector<int>& values, int32_t value) const
    {
        values.push_back(static_cast<int>(value));
    }

    void ReflectionSampleComponent::MakeGreeting(const std::string& name, std::string& outGreeting) const
    {
        outGreeting = "Hello, " + name;
    }

    std::string ReflectionSampleComponent::GetGreeting(const std::string& name) const
    {
        return "Hello, " + name;
    }

    int32_t ReflectionSampleComponent::SumVectorValue(std::vector<int> values) const
    {
        int32_t sum = 0;
        for (int value : values)
        {
            sum += value;
        }
        return sum;
    }

    int32_t ReflectionSampleComponent::CountGreetingChars(std::string greeting) const
    {
        return static_cast<int32_t>(greeting.size());
    }

    int32_t ReflectionSampleComponent::SumSampleData(ReflectionSampleClass value) const
    {
        return value.IntField + static_cast<int32_t>(value.FloatField);
    }

    ReflectionSampleClass ReflectionSampleComponent::BuildSampleData(ReflectionSampleClass value) const
    {
        ReflectionSampleClass out = value;
        out.IntField += 1;
        out.FloatField += 2.0f;
        out.StringField = "Built:" + value.StringField;
        out.EnumField = ReflectionSampleEnum::ValueC;
        return out;
    }

    bool ReflectionSampleComponent::IsValidSharedComponent(std::shared_ptr<Component> candidate) const
    {
        return candidate != nullptr && candidate->IsA(Component::StaticClass());
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

    bool ReflectionSampleComponent::IsValidSharedComponentConstRef(const std::shared_ptr<Component>& candidate) const
    {
        return candidate != nullptr && candidate->IsA(Component::StaticClass());
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

    int32_t ReflectionSampleComponent::CountNames(std::vector<std::string> names) const
    {
        return static_cast<int32_t>(names.size());
    }

    std::vector<std::string> ReflectionSampleComponent::BuildNames(std::vector<std::string> input) const
    {
        std::vector<std::string> output = input;
        output.push_back("Tail");
        return output;
    }

    int32_t ReflectionSampleComponent::SumNested(std::vector<std::vector<int>> values) const
    {
        int32_t sum = 0;
        for (const std::vector<int>& row : values)
        {
            for (int value : row)
            {
                sum += value;
            }
        }
        return sum;
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

    void ReflectionSampleComponent::AppendName(std::vector<std::string>& names, const std::string& name) const
    {
        names.push_back(name);
    }

    void ReflectionSampleComponent::BuildNestedOut(int32_t n, std::vector<std::vector<int>>& outValues) const
    {
        outValues.clear();
        if (n <= 0)
        {
            return;
        }

        outValues.reserve(static_cast<size_t>(n));
        for (int32_t rowIndex = 0; rowIndex < n; ++rowIndex)
        {
            std::vector<int> row;
            row.reserve(static_cast<size_t>(rowIndex + 1));
            for (int32_t colIndex = 0; colIndex <= rowIndex; ++colIndex)
            {
                row.push_back(static_cast<int>(rowIndex * 10 + colIndex));
            }
            outValues.push_back(row);
        }
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
