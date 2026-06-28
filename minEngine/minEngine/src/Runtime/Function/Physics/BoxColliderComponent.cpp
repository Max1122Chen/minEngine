#include "BoxColliderComponent.h"

#include "RigidBodyComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    void BoxColliderComponent::SetHalfExtent(const Vector3& halfExtent)
    {
        if (m_HalfExtent == halfExtent)
        {
            return;
        }

        m_HalfExtent = halfExtent;

        if (m_Owner == nullptr)
        {
            return;
        }

        for (const std::shared_ptr<Component>& component : m_Owner->GetAllComponents())
        {
            if (component && component->GetClass() && component->IsA(RigidBodyComponent::StaticClass()))
            {
                static_cast<RigidBodyComponent*>(component.get())->RefreshPhysicsBody(this);
                break;
            }
        }
    }

    void BoxColliderComponent::SetOwner(GameObject* inOwner)
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
