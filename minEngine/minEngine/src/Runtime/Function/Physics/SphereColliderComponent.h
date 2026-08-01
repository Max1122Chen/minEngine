#pragma once

#include "Core.h"
#include "Runtime/Function/Physics/ColliderComponent.h"

namespace minEngine
{
    ME_CLASS()
    class SphereColliderComponent : public ColliderComponent
    {
        ME_GENERATED_BODY(SphereColliderComponent)

    public:
        SphereColliderComponent() = default;
        ~SphereColliderComponent() override = default;

        float GetRadius() const { return m_Radius; }
        void SetRadius(float radius);

        void SetOwner(GameObject* inOwner) override;

    private:
        ME_PROPERTY(EditAnywhere)
        float m_Radius{0.5f};
    };
}

#include "Generated/Reflection/SphereColliderComponent.gen.h"
