#pragma once
#include "Core.h"

namespace minEngine
{
    class GameObject;

    enum ComponentMarkedForNeededEndOfFrameUpdate
    {
        Marked,
        Unmarked
    };


    class Component
    {
    public:
        Component() = default;
        Component(std::shared_ptr<GameObject> owner) : m_Owner(owner) {}
        virtual ~Component() = default;

        virtual void Tick(float deltaTime) {}

        void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }
        std::shared_ptr<GameObject> GetOwner() const { return m_Owner; }

        bool CanEverTick() const { return m_bCanEverTick; }

        void MarkForNeededEndOfFrameUpdate();
        uint32_t GetMarkedForNeededEndOfFrameUpdate() const { return m_MarkedForNeededEndOfFrameUpdate; }
        void SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate marked) { m_MarkedForNeededEndOfFrameUpdate = marked; }
    
    protected:
        std::shared_ptr<GameObject> m_Owner{ nullptr };
        bool m_bCanEverTick{ true };
        ComponentMarkedForNeededEndOfFrameUpdate m_MarkedForNeededEndOfFrameUpdate{ Unmarked };

    };
}
