#pragma once
#include "Core.h"
#include "Math.h"
#include "Runtime/Function/Framework/Components/Component.h"

namespace minEngine
{
    ME_ENUM()
    enum ReflectionSampleEnum
    {
        ValueA,
        ValueB,
        ValueC
    };

    ME_CLASS(Transient, meta = (Category = "ReflectionSample", DisplayName = "ReflectionSampleClass"))
    class MINENGINE_API ReflectionSampleClass
    {
        ME_GENERATED_BODY(ReflectionSampleClass)
    public:
        ME_PROPERTY(Transient, meta = (Category = "Sample", DisplayName = "Sample Int"), EditAnywhere)
        int IntField = 42;

        ME_PROPERTY(EditAnywhere)
        float FloatField = 3.14f;

        ME_PROPERTY(EditAnywhere)
        std::string StringField = "Hello, Reflection!";

        ME_PROPERTY(EditAnywhere)
        ReflectionSampleEnum EnumField = ReflectionSampleEnum::ValueB;
    };

    ME_CLASS()
    class ReflectionSampleComponent : public Component
    {
        ME_GENERATED_BODY(ReflectionSampleComponent)
    public:
        void ResetCounter();
        int32_t GetCounter() const;
        int32_t Add(int32_t firstOperand, int32_t secondOperand);
        void SetFunctionTestCounter(int32_t value) { m_FunctionTestCounter = value; }

        ME_PROPERTY(EditAnywhere)
        ReflectionSampleClass SampleData;

        ME_PROPERTY(EditAnywhere)
        std::vector<int> IntArray{ 1, 2, 3, 4, 5 };

    private:
        int32_t m_FunctionTestCounter = 0;
    };
}

#include "ReflectionSample.gen.h"

