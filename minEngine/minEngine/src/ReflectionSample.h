#pragma once
#include "Core.h"

namespace minEngine
{
    ME_ENUM()
    enum ReflectionSampleEnum
    {
        ValueA,
        ValueB,
        ValueC
    };

    ME_CLASS()
    class ReflectionSampleClass
    {
    public:
        ME_PROPERTY(EditAnywhere)
        int IntField = 42;

        ME_PROPERTY(EditAnywhere)
        float FloatField = 3.14f;

        ME_PROPERTY(EditAnywhere)
        std::string StringField = "Hello, Reflection!";

        ME_PROPERTY(EditAnywhere)
        ReflectionSampleEnum EnumField = ReflectionSampleEnum::ValueB;
    };
}

#include "ReflectionSample.gen.h"