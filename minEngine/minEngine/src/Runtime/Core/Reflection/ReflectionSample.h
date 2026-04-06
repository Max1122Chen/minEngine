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

    ME_CLASS()
    class ReflectionSampleComponent : public Component
    {
    public:
        ME_PROPERTY(EditAnywhere) 
        Vector3 Position;

        ME_PROPERTY(EditAnywhere)
        Vector3 Rotation;

        ME_PROPERTY(EditAnywhere)
        Vector3 Scale = Vector3(1.0f);
    };
}

#include "ReflectionSample.gen.h"