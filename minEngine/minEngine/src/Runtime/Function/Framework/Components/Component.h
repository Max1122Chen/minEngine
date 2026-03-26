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

    /**
     * @brief 
     * Base class for all components that can be attached to GameObjects.
     */
    class Component
    {
    public:
        Component();
        virtual ~Component() = default;

        virtual void Tick(float deltaTime) {}

        virtual void SetOwner(GameObject* inOwner);
        GameObject* GetOwner() const { return m_Owner; }

        bool CanEverTick() const { return m_bCanEverTick; }

        void MarkForNeededEndOfFrameUpdate();
        uint32_t GetMarkedForNeededEndOfFrameUpdate() const { return m_MarkedForNeededEndOfFrameUpdate; }
        void SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate marked) { m_MarkedForNeededEndOfFrameUpdate = marked; }
        virtual void DoEndOfFrameUpdate() {}
    
    protected:
        GameObject* m_Owner{ nullptr };
        bool m_bCanEverTick{ true };
        ComponentMarkedForNeededEndOfFrameUpdate m_MarkedForNeededEndOfFrameUpdate{ Unmarked };

    };
}
