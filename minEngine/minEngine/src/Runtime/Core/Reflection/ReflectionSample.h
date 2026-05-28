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
        ME_FUNCTION()
        void ResetCounter();
        ME_FUNCTION()
        int32_t GetCounter() const;
        ME_FUNCTION()
        int32_t Add(int32_t firstOperand, int32_t secondOperand);
        ME_FUNCTION()
        bool MixAlign(bool inputFlag, double threshold) const;
        ME_FUNCTION()
        void AddInPlace(int32_t& value, int32_t delta);
        ME_FUNCTION()
        void PeekString(const std::string& input, int32_t& outLength) const;
        ME_FUNCTION()
        void FillOut(int32_t value, int32_t& outValue);
        ME_FUNCTION()
        ReflectionSampleEnum EchoEnum(ReflectionSampleEnum value) const;
        ME_FUNCTION()
        int32_t SumIntArray(const std::vector<int>& values) const;
        ME_FUNCTION()
        bool IsSameObject(MEObject* a, MEObject* b) const;
        ME_FUNCTION()
        static void StaticResetCounter();
        ME_FUNCTION()
        static int32_t StaticGetCounter();
        ME_FUNCTION()
        static int32_t StaticAdd(int32_t firstOperand, int32_t secondOperand);
        ME_FUNCTION()
        static void StaticAddInPlace(int32_t& value, int32_t delta);
        void SetFunctionTestCounter(int32_t value) { m_FunctionTestCounter = value; }
        static void SetStaticTestCounter(int32_t value) { s_StaticTestCounter = value; }

        ME_PROPERTY(EditAnywhere)
        ReflectionSampleClass SampleData;

        ME_PROPERTY(EditAnywhere)
        std::vector<int> IntArray{ 1, 2, 3, 4, 5 };

    private:
        int32_t m_FunctionTestCounter = 0;
        static inline int32_t s_StaticTestCounter = 0;
    };
}

#include "ReflectionSample.gen.h"

