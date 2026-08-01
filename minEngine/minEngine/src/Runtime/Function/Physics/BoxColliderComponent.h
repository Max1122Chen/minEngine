#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Physics/ColliderComponent.h"

namespace minEngine
{
    class RigidBodyComponent;

    ME_CLASS()
    class BoxColliderComponent : public ColliderComponent
    {
        ME_GENERATED_BODY(BoxColliderComponent)

    public:
        BoxColliderComponent() = default;
        ~BoxColliderComponent() override = default;

        const Vector3& GetHalfExtent() const { return m_HalfExtent; }
        void SetHalfExtent(const Vector3& halfExtent);

        /** Full box size (2 * HalfExtent). Matches unit cube mesh when HalfExtent is (0.5,0.5,0.5). */
        Vector3 GetFullExtent() const { return m_HalfExtent * 2.0f; }
        void SetFullExtent(const Vector3& fullExtent);

        void SetOwner(GameObject* inOwner) override;

    private:
        /** Half-extents in engine units (meters); default matches Assets/.../cube.obj (±0.5). */
        ME_PROPERTY(EditAnywhere)
        Vector3 m_HalfExtent{0.5f, 0.5f, 0.5f};
    };
}

#include "Generated/Reflection/BoxColliderComponent.gen.h"
