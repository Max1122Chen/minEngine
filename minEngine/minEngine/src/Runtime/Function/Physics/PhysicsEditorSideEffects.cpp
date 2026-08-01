#include "PhysicsEditorSideEffects.h"

#include "BoxColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "ColliderComponent.h"
#include "RigidBodyComponent.h"
#include "SphereColliderComponent.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    namespace
    {
        bool PropertyPathStartsWith(std::string_view propertyPath, std::string_view prefix)
        {
            if (propertyPath.size() < prefix.size())
            {
                return false;
            }

            if (propertyPath.compare(0, prefix.size(), prefix) != 0)
            {
                return false;
            }

            return propertyPath.size() == prefix.size()
                || propertyPath[prefix.size()] == '.';
        }

        void RefreshRigidBodyOnOwner(GameObject* owner, ColliderComponent* colliderOverride)
        {
            if (owner == nullptr)
            {
                return;
            }

            for (const std::shared_ptr<Component>& component : owner->GetAllComponents())
            {
                if (component && component->GetClass() && component->IsA(RigidBodyComponent::StaticClass()))
                {
                    static_cast<RigidBodyComponent*>(component.get())->RefreshPhysicsBody(colliderOverride);
                    return;
                }
            }
        }
    }

    void ApplyPhysicsEditorSideEffects(MEObject* owner, std::string_view propertyPath)
    {
        if (owner == nullptr || propertyPath.empty())
        {
            return;
        }

        if (owner->IsA(SceneComponent::StaticClass())
            && PropertyPathStartsWith(propertyPath, "m_Transform"))
        {
            static_cast<SceneComponent*>(owner)->ApplyEditorTransformEdit();
            return;
        }

        if (!owner->IsA(Component::StaticClass()))
        {
            return;
        }

        Component* component = static_cast<Component*>(owner);

        if (component->IsA(RigidBodyComponent::StaticClass()))
        {
            RigidBodyComponent* rigidBody = static_cast<RigidBodyComponent*>(component);
            if (propertyPath == "m_bSimulatePhysics")
            {
                rigidBody->ApplySimulatePhysicsToWorld();
            }
            else if (propertyPath == "m_BodyType" || propertyPath == "m_Mass")
            {
                rigidBody->RefreshPhysicsBody();
            }
            return;
        }

        if (!component->IsA(ColliderComponent::StaticClass()))
        {
            return;
        }

        ColliderComponent* collider = static_cast<ColliderComponent*>(component);
        if (propertyPath == "m_ObjectChannel"
            || propertyPath == "m_HalfExtent"
            || propertyPath == "m_Radius"
            || propertyPath == "m_HalfHeight")
        {
            RefreshRigidBodyOnOwner(component->GetOwner(), collider);
        }
    }
}
