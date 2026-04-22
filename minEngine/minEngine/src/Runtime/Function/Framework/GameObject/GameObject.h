#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    struct Transform;

    class Component;
    class SceneComponent;

    ME_CLASS()
    class GameObject : public MEObject
    {
        ME_REFLECTION_FRIEND(GameObject)
    public:
        GameObject();
        virtual ~GameObject() = default;

        void Tick(float deltaTime);

        uint64_t GetID() const { return m_ID; }
        void SetID(uint64_t id) { m_ID = id; }

        Transform GetTransform();
        void SetTransform(const Transform& inTransform);

        Vector3 GetPosition();
        void SetPosition(const Vector3& position);

        Vector3 GetRotation();
        void SetRotation(const Vector3& rotation);

        Vector3 GetScale();
        void SetScale(const Vector3& scale);

    
        std::shared_ptr<SceneComponent> GetRootComponent() const { return m_RootComponent; }
        void SetRootComponent(std::shared_ptr<SceneComponent> rootComponent) { m_RootComponent = rootComponent; }
        std::vector<std::shared_ptr<Component>>& GetComponents() { return m_Components; }

        // just a simple implementation for demo purposes
        template<typename T>
        std::shared_ptr<T> GetComponent()
        {
            for(auto& component : m_Components)
            {
                std::shared_ptr<T> castedComponent = std::dynamic_pointer_cast<T>(component);
                if(castedComponent)
                {
                    return castedComponent;
                }
            }
            return nullptr;
        }
        
        template<typename T>
        std::shared_ptr<T> AddComponent()
        {
            std::shared_ptr<T> newComponent = NewObject<T>("",this);
            newComponent->SetOwner(this);
            m_Components.push_back(newComponent);
            // TODO: attach to root component by default if it's a SceneComponent, and handle the case when root component is missing

            return newComponent;
        }

        std::shared_ptr<Component> AddComponent(const std::string& componentTypeName);

    private:
        uint64_t m_ID{ 0 };

        ME_PROPERTY()
        std::shared_ptr<SceneComponent> m_RootComponent{ nullptr };

        ME_PROPERTY(Instanced)
        std::vector<std::shared_ptr<Component>> m_Components;

    };
}

#include "GameObject.gen.h"