#include "RigidBodyComponent.h"

#include "BoxColliderComponent.h"
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

    BoxColliderComponent* RigidBodyComponent::FindBoxColliderComponent() const
    {
        if (m_Owner == nullptr)
        {
            return nullptr;
        }

        for (const std::shared_ptr<Component>& component : m_Owner->GetAllComponents())
        {
            if (component && component->GetClass() && component->IsA(BoxColliderComponent::StaticClass()))
            {
                return static_cast<BoxColliderComponent*>(component.get());
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

    void RigidBodyComponent::SetOwner(GameObject* inOwner)
    {
        Component::SetOwner(inOwner);
        RefreshPhysicsBody();
    }

    void RigidBodyComponent::RefreshPhysicsBody(BoxColliderComponent* boxColliderOverride)
    {
        DestroyPhysicsBody();

        if (m_Owner == nullptr || !PhysicsSystem::HasInstance())
        {
            return;
        }

        const MEObject* outer = m_Owner->GetOuter();
        if (outer == nullptr || !outer->IsA(Scene::StaticClass()))
        {
            return;
        }

        BoxColliderComponent* boxCollider = boxColliderOverride != nullptr ? boxColliderOverride : FindBoxColliderComponent();
        if (boxCollider == nullptr || GetTargetSceneComponent() == nullptr)
        {
            return;
        }

        Scene* scene = const_cast<Scene*>(static_cast<const Scene*>(outer));
        PhysicsSystem::Get().GetOrCreateWorld(scene).RegisterRigidBody(this, boxCollider);
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
