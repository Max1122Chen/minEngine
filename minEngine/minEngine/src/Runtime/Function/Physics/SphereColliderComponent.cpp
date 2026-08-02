#include "SphereColliderComponent.h"

#include "RigidBodyComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    void SphereColliderComponent::SetRadius(float radius)
    {
        if (m_Radius == radius)
        {
            return;
        }

        m_Radius = radius;
        RefreshOwningRigidBody();
    }

    void SphereColliderComponent::SetOwner(GameObject* inOwner)
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
