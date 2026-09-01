#include "ColliderComponent.h"

#include "RigidBodyComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    ColliderComponent::~ColliderComponent()
    {
        RefreshOwningRigidBody();
    }

    void ColliderComponent::ApplyActivationToSystems()
    {
        RefreshOwningRigidBody();
    }

    void ColliderComponent::RemoveActivationFromSystems()
    {
        RefreshOwningRigidBody();
    }

    void ColliderComponent::SetObjectChannel(ECollisionChannel objectChannel)
    {
        if (m_ObjectChannel == objectChannel)
        {
            return;
        }

        m_ObjectChannel = objectChannel;
        RefreshOwningRigidBody();
    }

    void ColliderComponent::RefreshOwningRigidBody()
    {
        if (m_Owner == nullptr)
        {
            return;
        }

        for (const std::shared_ptr<Component>& component : m_Owner->GetAllComponents())
        {
            if (component && component->GetClass() && component->IsA(RigidBodyComponent::StaticClass()))
            {
                static_cast<RigidBodyComponent*>(component.get())->RefreshPhysicsBody();
                return;
            }
        }
    }
}
