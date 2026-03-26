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
        virtual ~PrimitiveComponent() override;

        void SetCastShadow(bool bInCastShadow)
        {
            if (m_CastShadow != bInCastShadow)
            {
                m_CastShadow = bInCastShadow;
                MarkRenderStateDirty();
            }
        }

        bool CastShadow() const { return m_CastShadow; }

        virtual void DoEndOfFrameUpdate() override;

        virtual PrimitiveSceneProxy* CreateSceneProxy() = 0;
        PrimitiveSceneProxy* GetSceneProxy() const { return m_SceneProxy; }

    protected:
        bool m_CastShadow{ true };


        PrimitiveSceneProxy* m_SceneProxy{ nullptr };
    };
}