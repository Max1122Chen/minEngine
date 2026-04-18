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

        const Transform& GetTransform();
        void SetTransform(const Transform& inTransform);

        const Vector3& GetPosition();
        void SetPosition(const Vector3& position);

        const Vector3& GetRotation();
        void SetRotation(const Vector3& rotation);

        const Vector3& GetScale();
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

            return newComponent;
        }

    private:
        uint64_t m_ID{ 0 };
        std::shared_ptr<SceneComponent> m_RootComponent{ nullptr };

        ME_PROPERTY()
        std::vector<std::shared_ptr<Component>> m_Components;

    };
}

#include "GameObject.gen.h"