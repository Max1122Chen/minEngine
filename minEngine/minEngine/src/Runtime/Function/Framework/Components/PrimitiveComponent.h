#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Math/Geometry/AABB.h"

namespace minEngine
{
    class SceneComponent;
    class PrimitiveSceneProxy;

    ME_CLASS(Abstract)
    class MINENGINE_API PrimitiveComponent : public SceneComponent
    {
        ME_GENERATED_BODY()
    public:
        PrimitiveComponent();
        virtual ~PrimitiveComponent() override;

        void SetCastShadow(bool bInCastShadow)
        {
            if (m_CastShadow != bInCastShadow)
            {
                m_CastShadow = bInCastShadow;
                MarkRenderTransformDirty();
            }
        }

        bool CastShadow() const { return m_CastShadow; }

        /**
         * Bounds in the component's own space, i.e. before its world transform.
         *
         * The world box the render role uses is derived from this once per sync, on the game role,
         * and stored on the proxy; nothing on the render role calls back in here.
         */
        virtual Math::Geometry::AABB GetBoundingBox() const = 0;

        virtual void DoEndOfFrameUpdate() override;

        virtual PrimitiveSceneProxy* CreateSceneProxy() = 0;
        PrimitiveSceneProxy* GetSceneProxy() const { return m_SceneProxy; }

        /** Drop the GT link before Recreate Create; RT still owns the old proxy until Remove_RT. */
        void DetachSceneProxy() { m_SceneProxy = nullptr; }

        /**
         * UE-style gate: true → Remove+Create+Add; false → Transform small-packet update.
         * Default: any render-state dirty this frame requires recreate.
         */
        virtual bool ShouldRecreateSceneProxy() const { return m_bRenderStateDirty; }

        /** Fill a newly created proxy on the game thread (Add/Recreate only — not UpdateTransform). */
        virtual void SyncSceneProxy(PrimitiveSceneProxy& proxy);

    protected:
        void ApplyActivationToSystems() override;
        void RemoveActivationFromSystems() override;

    protected:
        ME_PROPERTY()
        bool m_CastShadow{ true };


        PrimitiveSceneProxy* m_SceneProxy{ nullptr };
    };
}

#include "Generated/Reflection/PrimitiveComponent.gen.h"