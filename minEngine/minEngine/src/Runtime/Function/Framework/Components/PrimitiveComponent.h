#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"


namespace minEngine
{
    class Component;
    class SceneComponent;
    class PrimitiveSceneProxy;

    class PrimitiveComponent : public SceneComponent
    {
    public:
        PrimitiveComponent() = default;
        PrimitiveComponent(std::shared_ptr<GameObject> owner) : SceneComponent(owner) {}
        virtual ~PrimitiveComponent() = default;

        void MarkRenderStateDirty();

        void SetTransform(const Transform& inTransform);

        virtual PrimitiveSceneProxy* CreateSceneProxy() = 0;
        PrimitiveSceneProxy* GetSceneProxy() const { return m_SceneProxy; }

    protected:
        bool m_bRenderStateDirty{ false };

        PrimitiveSceneProxy* m_SceneProxy{ nullptr };
    };
}