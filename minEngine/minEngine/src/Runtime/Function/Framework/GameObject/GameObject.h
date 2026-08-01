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
        ME_GENERATED_BODY(GameObject)
    public:
        GameObject();
        virtual ~GameObject()
        {
            // A simple implementation for demo purposes. We will handle component ownership and lifecycle more robustly in the future.
            // TODO: handle lifecycle of components more robustly, we may want to implement a component system that can manage the lifecycle of components instead of relying on the game object to destroy them, we may also want to implement a reference counting system for components to avoid dangling pointers.
            for (auto& component : m_Components)
            {
                if (component)
                {
                    component->SetOwner(nullptr);
                    component.reset();
                }
            }
            m_Components.clear();
            ME_CORE_INFO("GameObject with ID {} and name '{}' is being destroyed.", m_ID, GetName());
        }

        void Tick(float deltaTime);

        uint64_t GetID() const { return m_ID; }
        void SetID(uint64_t id) { m_ID = id; }

        void Rename(const std::string& newName) { SetName(newName); }

        Transform GetTransform();
        void SetTransform(const Transform& inTransform);

        ME_FUNCTION(ScriptCallable)
        Vector3 GetPosition();
        ME_FUNCTION(ScriptCallable)
        void SetPosition(const Vector3& position);
        ME_FUNCTION(ScriptCallable)
        void Translate(const Vector3& delta);

        Vector3 GetRotation();
        void SetRotation(const Vector3& rotation);
        void Rotate(const glm::quat& delta, Space relativeTo = Space::Local);

        Vector3 GetScale();
        void SetScale(const Vector3& scale);
        void ScaleBy(const Vector3& scaleFactor);

    
        SceneComponent* GetRootComponent() const { return m_RootComponent; }
        void SetRootComponent(SceneComponent* rootComponent) { m_RootComponent = rootComponent; }
        std::vector<std::shared_ptr<Component>>& GetAllComponents() { return m_Components; }

        // just a simple implementation for demo purposes
        template<typename T>
        std::vector<std::shared_ptr<T>> GetComponentsOfType()
        {
            std::vector<std::shared_ptr<T>> result;
            for(auto& component : m_Components)
            {
                component->GetClass()->IsA(T::StaticClass()) ? result.push_back(std::static_pointer_cast<T>(component)) : void();
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

        void InsertRestoredComponent(std::shared_ptr<Component> component, size_t index);

    private:
        void AddComponent_Internal(std::shared_ptr<Component> newComponent);

    private:
        uint64_t m_ID{ 0 };

        ME_PROPERTY()
        SceneComponent* m_RootComponent{ nullptr };

        ME_PROPERTY(Instanced)
        std::vector<std::shared_ptr<Component>> m_Components;

    };
}

#include "GameObject.gen.h"