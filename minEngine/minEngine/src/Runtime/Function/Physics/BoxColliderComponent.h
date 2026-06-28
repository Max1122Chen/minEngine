#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/Component.h"

namespace minEngine
{
    class RigidBodyComponent;

    ME_CLASS()
    class BoxColliderComponent : public Component
    {
        ME_GENERATED_BODY(BoxColliderComponent)

    public:
        BoxColliderComponent() = default;
        ~BoxColliderComponent() override = default;

        const Vector3& GetHalfExtent() const { return m_HalfExtent; }
        void SetHalfExtent(const Vector3& halfExtent);

        void SetOwner(GameObject* inOwner) override;

    private:
        ME_PROPERTY(EditAnywhere)
        Vector3 m_HalfExtent{0.5f, 0.5f, 0.5f};
    };
}

#include "Generated/Reflection/BoxColliderComponent.gen.h"
