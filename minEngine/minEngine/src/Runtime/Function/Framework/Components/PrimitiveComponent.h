#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Math/Geometry/AABB.h"

namespace minEngine
{
    class SceneComponent;
    class PrimitiveSceneProxy;

    ME_CLASS()
    class PrimitiveComponent : public SceneComponent
    {
        ME_GENERATED_BODY(PrimitiveComponent)
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
        virtual Math::Geometry::AABB GetBoundingBox() const = 0;

        virtual void DoEndOfFrameUpdate() override;

        virtual PrimitiveSceneProxy* CreateSceneProxy() = 0;
        PrimitiveSceneProxy* GetSceneProxy() const { return m_SceneProxy; }

    protected:
        ME_PROPERTY()
        bool m_CastShadow{ true };


        PrimitiveSceneProxy* m_SceneProxy{ nullptr };
    };
}

#include "Generated/Reflection/PrimitiveComponent.gen.h"