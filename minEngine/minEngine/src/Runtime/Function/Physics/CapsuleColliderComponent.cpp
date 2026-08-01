#include "CapsuleColliderComponent.h"

#include "RigidBodyComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    void CapsuleColliderComponent::SetRadius(float radius)
    {
        if (m_Radius == radius)
        {
            return;
        }

        m_Radius = radius;
        RefreshOwningRigidBody();
    }

    void CapsuleColliderComponent::SetHalfHeight(float halfHeight)
    {
        if (m_HalfHeight == halfHeight)
        {
            return;
        }

        m_HalfHeight = halfHeight;
        RefreshOwningRigidBody();
    }

    void CapsuleColliderComponent::SetOwner(GameObject* inOwner)
    {
        Component::SetOwner(inOwner);
        if (inOwner == nullptr)
        {
            return;
        }

        for (const std::shared_ptr<Component>& component : inOwner->GetAllComponents())
        {
            if (component && component->GetClass() && component->IsA(RigidBodyComponent::StaticClass()))
            {
                static_cast<RigidBodyComponent*>(component.get())->RefreshPhysicsBody(this);
                break;
            }
        }
    }
}
