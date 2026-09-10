#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "TypeTraits.h"

namespace minEngine
{
    struct Transform;

    class Component;
    class SceneComponent;

    ME_CLASS(ScriptType)
    class GameObject : public MEObject
    {
        ME_GENERATED_BODY()
    public:
        GameObject();
        virtual ~GameObject();

        void Tick(float deltaTime);

        uint64_t GetID() const { return m_ID; }
        void SetID(uint64_t id) { m_ID = id; }

        void Rename(const std::string& newName) { SetName(newName); }

        Transform GetTransform();
        void SetTransform(const Transform& inTransform);
        Transform GetWorldTransform() const;
        void SetWorldTransform(const Transform& worldTransform);

        ME_FUNCTION(ScriptCallable)
        Vector3 GetPosition();
        ME_FUNCTION(ScriptCallable)
        void SetPosition(const Vector3& position);
        ME_FUNCTION(ScriptCallable)
        void Translate(const Vector3& delta);

        ME_FUNCTION(ScriptCallable)
        Component* FindComponentByClassName(const std::string& componentClassName) const;

        Quaternion GetRotation();
        void SetRotation(const Quaternion& rotation);
        Vector3 GetRotationEulerDegrees();
        void SetRotationEulerDegrees(const Vector3& rotationEulerDegrees);
        void Rotate(const glm::quat& delta, Space relativeTo = Space::Local);

        Vector3 GetScale();
        void SetScale(const Vector3& scale);
        void ScaleBy(const Vector3& scaleFactor);

    
        SceneComponent* GetRootComponent() const { return m_RootComponent; }
        /** CORE-F09: swapping Root rebinds GO-parent Root↔Root (KeepWorld) or dissolves edges. */
        void SetRootComponent(SceneComponent* rootComponent);
        std::vector<std::shared_ptr<Component>>& GetAllComponents() { return m_Components; }

        // just a simple implementation for demo purposes
        template<typename T>
        std::vector<std::shared_ptr<T>> GetComponentsOfType()
        {
            std::vector<std::shared_ptr<T>> result;
            for (auto& component : m_Components)
            {
                // Entries may be null mid-destruction (SetOwner(nullptr) then reset).
                if (!component || component->GetClass() == nullptr)
                {
                    continue;
                }

                if (component->GetClass()->IsA(T::StaticClass()))
                {
                    result.push_back(std::static_pointer_cast<T>(component));
                }
            }
            return result;
        }
        
        template<typename T>
        std::shared_ptr<T> AddComponent()
        {
            static_assert(std::is_base_of_v<minEngine::Component,T>,"Tried to use AddComponent<T> with a non-component type!!!");
            std::shared_ptr<T> newComponentBase = NewObject<T>("",this);
            std::shared_ptr<Component> newComponent = std::static_pointer_cast<Component>(newComponentBase);
            AddComponent_Internal(newComponent);
            return newComponentBase;
        }

        std::shared_ptr<Component> AddComponent(const std::string& componentTypeName);

        bool RemoveComponent(Component& target);

        /** Reorder m_Components. Root SceneComponent cannot be moved. Returns false on failure. */
        bool MoveComponent(Component& target, size_t newIndex);
        size_t FindComponentIndex(const Component& target) const;

        void InsertRestoredComponent(std::shared_ptr<Component> component, size_t index);

        /**
         * CORE-F09: GO parent means child Root attaches under parent Root.
         * Both GOs must have a Root SceneComponent; otherwise returns false.
         */
        bool AttachToParent(GameObject* parent, AttachmentTransformRules rules);
        void DetachFromParent(AttachmentTransformRules rules);
        GameObject* GetParent() const { return m_Parent; }
        const std::vector<GameObject*>& GetChildren() const { return m_Children; }

        /** Clear m_Children only (keeps serialized m_Parent for Resolve). */
        void ClearChildrenLinks();

    private:
        void AddComponent_Internal(std::shared_ptr<Component> newComponent);
        bool WouldCreateHierarchyCycle(const GameObject* candidateParent) const;
        void UnlinkFromCurrentParent();
        void RebindRootToGameObjectParent(AttachmentTransformRules rules);
        void RebindChildGameObjectRoots(AttachmentTransformRules rules);

    private:
        /** Runtime scene-local lookup id; not used for hierarchy serialization. */
        uint64_t m_ID{ 0 };

        /** Serialized parent (GUID); null = scene root. Children rebuilt on load. */
        ME_PROPERTY()
        GameObject* m_Parent{ nullptr };

        std::vector<GameObject*> m_Children;

        ME_PROPERTY()
        SceneComponent* m_RootComponent{ nullptr };

        ME_PROPERTY(Instanced)
        std::vector<std::shared_ptr<Component>> m_Components;

    };
}

#include "GameObject.gen.h"