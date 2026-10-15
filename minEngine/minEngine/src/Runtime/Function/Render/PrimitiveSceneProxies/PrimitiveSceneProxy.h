#pragma once
#include "Core.h"
#include "Math/Geometry/AABB.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

namespace minEngine
{
    class PrimitiveComponent;
    class MaterialRenderProxy;
    class RHIBuffer;
    class RHIVertexInputLayout;

    /**
     * Scene representation of a drawable primitive.
     * Current subclasses (StaticMesh / SkeletalMesh / Sprite) all submit a mesh,
     * so vertex buffers and the material proxy live here. Non-mesh proxies
     * (Widget, lights, sky) do not inherit this type.
     */
    class PrimitiveSceneProxy
    {
    public:
        PrimitiveSceneProxy() = default;
        virtual ~PrimitiveSceneProxy() = default;

        PrimitiveComponent* m_PrimitiveComponent{ nullptr };
        Transform m_Transform;
        bool m_CastShadow = true;

        /** Non-owning; GPU buffers are owned by the mesh/quad asset. */
        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;

        /** Non-owning; owned by Material (or later MaterialInstance). */
        MaterialRenderProxy* m_MaterialProxy = nullptr;

        /**
         * World-space AABB for visibility/sorting (+).
         * Default: component local box transformed by m_Transform.
         */
        virtual Math::Geometry::AABB ResolveWorldBounds() const;

        /** Local→world used for ResolveWorldBounds (Sprite overrides with m_ModelMatrix). */
        virtual Matrix4 GetBoundsLocalToWorld() const;

        /** Emit pass-agnostic mesh descriptions. Default: one batch from the fields above when drawable. */
        virtual void GetMeshBatches(class MeshBatchCollector& collector) const;
    };
}
