#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Math/Geometry/AABB.h"

namespace minEngine
{
    class MaterialRenderProxy;
    class PrimitiveSceneProxy;
    class RHIBuffer;
    class RHIVertexInputLayout;

    /** Pass-agnostic mesh description emitted by a PrimitiveSceneProxy. */
    class MeshBatch
    {
    public:
        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;

        MaterialRenderProxy* m_MaterialProxy = nullptr;

        Matrix4 m_LocalToWorld{ 1.0f };
        Math::Geometry::AABB m_BoundingBox;
        bool m_CastShadow = true;

        const Matrix4* m_BonePalette = nullptr;
        uint32_t m_BoneCount = 0;
        bool m_bSkinned = false;

        /** Sprite (and similar) can force the translucent queue without a translucent material. */
        bool m_bForceTranslucentPass = false;

        const PrimitiveSceneProxy* m_Proxy = nullptr;
    };
}
