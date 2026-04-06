#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    struct Transform;

    class Component;
    class SceneComponent;

    class GameObject
    {
    public:
        explicit GameObject(uint64_t id, std::string name = "");
        virtual ~GameObject() = default;

        void Tick(float deltaTime);

        uint64_t m_ID{ 0 };

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        const Transform& GetTransform() { return m_RootComponent->GetTransform(); }     // TODO: change this dangerous design later
        void SetTransform(const Transform& inTransform) { m_RootComponent->SetTransform(inTransform); }

        const Vector3& GetPosition() { return m_RootComponent->GetPosition(); }
        void SetPosition(const Vector3& position) { m_RootComponent->SetPosition(position); }

        const Vector3& GetRotation() { return m_RootComponent->GetRotation(); }
        void SetRotation(const Vector3& rotation) { m_RootComponent->SetRotation(rotation); }

        const Vector3& GetScale() { return m_RootComponent->GetScale(); }
        void SetScale(const Vector3& scale) { m_RootComponent->SetScale(scale); }

    
        std::shared_ptr<SceneComponent> GetRootComponent() const { return m_RootComponent; }
        void SetRootComponent(const std::shared_ptr<SceneComponent>& rootComponent) { m_RootComponent = rootComponent; }
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
        std::shared_ptr<T> CreateAndAddComponent()
        {
            std::shared_ptr<T> newComponent = std::make_shared<T>();
            newComponent->SetOwner(this);
            m_Components.push_back(newComponent);

            return newComponent;
        }

    private:
        std::string m_Name;
        std::shared_ptr<SceneComponent> m_RootComponent{ nullptr };
        std::vector<std::shared_ptr<Component>> m_Components;

    };

    
}