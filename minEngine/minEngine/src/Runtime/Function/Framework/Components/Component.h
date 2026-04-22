#pragma once
#include "Core.h"
#include "Core/Object/MEObject.h"

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
    ME_CLASS()
    class Component : public MEObject
    {
        ME_REFLECTION_FRIEND(Component)
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
        ME_PROPERTY(Invisible)
        GameObject* m_Owner{ nullptr };
        bool m_bCanEverTick{ true };
        ComponentMarkedForNeededEndOfFrameUpdate m_MarkedForNeededEndOfFrameUpdate{ Unmarked };

    };
}

#include "Generated/Reflection/Component.gen.h"
