#pragma once

#include "Core.h"
#include "Runtime/Function/Physics/ColliderComponent.h"

namespace minEngine
{
    /**
     * Capsule along engine +Y (up). HalfHeight is the cylinder half-height (Jolt semantics);
     * total height ~= 2 * (HalfHeight + Radius).
     */
    ME_CLASS()
    class CapsuleColliderComponent : public ColliderComponent
    {
        ME_GENERATED_BODY(CapsuleColliderComponent)

    public:
        CapsuleColliderComponent() = default;
        ~CapsuleColliderComponent() override = default;

        float GetRadius() const { return m_Radius; }
        void SetRadius(float radius);

        float GetHalfHeight() const { return m_HalfHeight; }
        void SetHalfHeight(float halfHeight);

        void SetOwner(GameObject* inOwner) override;

    private:
        ME_PROPERTY(EditAnywhere)
        float m_Radius{0.5f};

        ME_PROPERTY(EditAnywhere)
        float m_HalfHeight{0.5f};
    };
}

#include "Generated/Reflection/CapsuleColliderComponent.gen.h"
