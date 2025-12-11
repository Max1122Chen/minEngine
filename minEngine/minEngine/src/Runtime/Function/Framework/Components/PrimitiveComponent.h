#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    class SceneComponent;
    class PrimitiveSceneProxy;

    class PrimitiveComponent : public SceneComponent
    {
    public:
        PrimitiveComponent();
        virtual ~PrimitiveComponent() = default;


        virtual void DoEndOfFrameUpdate() override;

        virtual PrimitiveSceneProxy* CreateSceneProxy() = 0;
        PrimitiveSceneProxy* GetSceneProxy() const { return m_SceneProxy; }

    protected:


        PrimitiveSceneProxy* m_SceneProxy{ nullptr };
    };
}