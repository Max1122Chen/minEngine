#pragma once
#include "Core.h"
#include "Component.h"
#include "Runtime/Core/Math/Math.h"


namespace minEngine
{
    ME_CLASS()
    class MovementComponent : public Component
    {
        ME_GENERATED_BODY(MovementComponent)
    public:
        MovementComponent() = default;
        virtual ~MovementComponent() = default;

        void AddMovementInput(const Vector3& worldDirection, float scale);
        void AddRotationInput(const Vector3& deltaRotation);
    };
}

#include "Generated/Reflection/MovementComponent.gen.h"