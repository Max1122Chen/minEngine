#include "PhysicsEditorSideEffects.h"

#include "BoxColliderComponent.h"
#include "ColliderComponent.h"
#include "RigidBodyComponent.h"
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

        void RefreshRigidBodyOnOwner(GameObject* owner, BoxColliderComponent* boxColliderOverride)
        {
            if (owner == nullptr)
            {
                return;
            }

            for (const std::shared_ptr<Component>& component : owner->GetAllComponents())
            {
                if (component && component->GetClass() && component->IsA(RigidBodyComponent::StaticClass()))
                {
                    static_cast<RigidBodyComponent*>(component.get())->RefreshPhysicsBody(boxColliderOverride);
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

        if (component->IsA(ColliderComponent::StaticClass()) && propertyPath == "m_ObjectChannel")
        {
            BoxColliderComponent* boxCollider = component->IsA(BoxColliderComponent::StaticClass())
                ? static_cast<BoxColliderComponent*>(component)
                : nullptr;
            RefreshRigidBodyOnOwner(component->GetOwner(), boxCollider);
            return;
        }

        if (component->IsA(BoxColliderComponent::StaticClass()) && propertyPath == "m_HalfExtent")
        {
            RefreshRigidBodyOnOwner(component->GetOwner(), static_cast<BoxColliderComponent*>(component));
        }
    }
}
