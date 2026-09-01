#include "RigidBodyComponent.h"

#include "ColliderComponent.h"
#include "PhysicsSystem.h"
#include "PhysicsWorld.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    RigidBodyComponent::~RigidBodyComponent()
    {
        DestroyPhysicsBody();
    }

    SceneComponent* RigidBodyComponent::GetTargetSceneComponent() const
    {
        if (m_Owner == nullptr)
        {
            return nullptr;
        }

        return m_Owner->GetRootComponent();
    }

    ColliderComponent* RigidBodyComponent::FindColliderComponent() const
    {
        if (m_Owner == nullptr)
        {
            return nullptr;
        }

        for (const std::shared_ptr<Component>& component : m_Owner->GetAllComponents())
        {
            if (component && component->GetClass() && component->IsA(ColliderComponent::StaticClass())
                && component->IsActive())
            {
                return static_cast<ColliderComponent*>(component.get());
            }
        }

        return nullptr;
    }

    void RigidBodyComponent::SetSimulatePhysics(bool simulatePhysics)
    {
        if (m_bSimulatePhysics == simulatePhysics)
        {
            return;
        }

        m_bSimulatePhysics = simulatePhysics;
        ApplySimulatePhysicsToWorld();
    }

    void RigidBodyComponent::ApplySimulatePhysicsToWorld()
    {
        if (!HasValidPhysicsBody() || m_Owner == nullptr || !PhysicsSystem::HasInstance())
        {
            return;
        }

        const MEObject* outer = m_Owner->GetOuter();
        if (outer == nullptr || !outer->IsA(Scene::StaticClass()))
        {
            return;
        }

        Scene* scene = const_cast<Scene*>(static_cast<const Scene*>(outer));
        PhysicsSystem::Get().GetOrCreateWorld(scene).OnRigidBodySimulatePhysicsChanged(this);
    }

    void RigidBodyComponent::ApplyActivationToSystems()
    {
        RefreshPhysicsBody();
    }

    void RigidBodyComponent::RemoveActivationFromSystems()
    {
        DestroyPhysicsBody();
    }

    void RigidBodyComponent::RefreshPhysicsBody(ColliderComponent* colliderOverride)
    {
        DestroyPhysicsBody();

        if (!IsActive() || m_Owner == nullptr || !PhysicsSystem::HasInstance())
        {
            return;
        }

        const MEObject* outer = m_Owner->GetOuter();
        if (outer == nullptr || !outer->IsA(Scene::StaticClass()))
        {
            return;
        }

        ColliderComponent* collider = colliderOverride;
        if (collider != nullptr && !collider->IsActive())
        {
            collider = nullptr;
        }

        if (collider == nullptr)
        {
            collider = FindColliderComponent();
        }

        if (collider == nullptr || GetTargetSceneComponent() == nullptr)
        {
            return;
        }

        Scene* scene = const_cast<Scene*>(static_cast<const Scene*>(outer));
        PhysicsSystem::Get().GetOrCreateWorld(scene).RegisterRigidBody(this, collider);
    }

    void RigidBodyComponent::DestroyPhysicsBody()
    {
        if (!HasValidPhysicsBody() || m_Owner == nullptr || !PhysicsSystem::HasInstance())
        {
            m_PhysicsBodyId = InvalidPhysicsBodyId;
            return;
        }

        const MEObject* outer = m_Owner->GetOuter();
        if (outer != nullptr && outer->IsA(Scene::StaticClass()))
        {
            Scene* scene = const_cast<Scene*>(static_cast<const Scene*>(outer));
            PhysicsSystem::Get().GetOrCreateWorld(scene).UnregisterRigidBody(this);
        }

        m_PhysicsBodyId = InvalidPhysicsBodyId;
    }
}
