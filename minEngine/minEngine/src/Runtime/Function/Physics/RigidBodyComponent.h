#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"

namespace minEngine
{
    class BoxColliderComponent;
    class PhysicsWorld;
    class SceneComponent;

    ME_CLASS()
    class RigidBodyComponent : public Component
    {
        ME_GENERATED_BODY(RigidBodyComponent)

    public:
        RigidBodyComponent() = default;
        ~RigidBodyComponent() override;

        EBodyType GetBodyType() const { return m_BodyType; }
        void SetBodyType(EBodyType bodyType) { m_BodyType = bodyType; }

        float GetMass() const { return m_Mass; }
        void SetMass(float mass) { m_Mass = mass; }

        bool GetSimulatePhysics() const { return m_bSimulatePhysics; }
        void SetSimulatePhysics(bool simulatePhysics);
        void ApplySimulatePhysicsToWorld();

        PhysicsBodyId GetPhysicsBodyId() const { return m_PhysicsBodyId; }
        bool HasValidPhysicsBody() const { return m_PhysicsBodyId != InvalidPhysicsBodyId; }

        SceneComponent* GetTargetSceneComponent() const;
        BoxColliderComponent* FindBoxColliderComponent() const;

        void SetOwner(GameObject* inOwner) override;
        void RefreshPhysicsBody(BoxColliderComponent* boxColliderOverride = nullptr);

    private:
        friend class PhysicsWorld;

        void SetPhysicsBodyId(PhysicsBodyId bodyId) { m_PhysicsBodyId = bodyId; }
        void DestroyPhysicsBody();

        ME_PROPERTY(EditAnywhere)
        EBodyType m_BodyType{EBodyType::Dynamic};

        ME_PROPERTY(EditAnywhere)
        float m_Mass{1.0f};

        ME_PROPERTY(EditAnywhere)
        bool m_bSimulatePhysics{true};

        ME_PROPERTY(Invisible)
        PhysicsBodyId m_PhysicsBodyId{InvalidPhysicsBodyId};
    };
}

#include "Generated/Reflection/RigidBodyComponent.gen.h"
