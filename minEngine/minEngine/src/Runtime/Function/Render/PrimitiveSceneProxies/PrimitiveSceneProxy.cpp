#include "PrimitiveSceneProxy.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"

namespace minEngine
{
    Matrix4 PrimitiveSceneProxy::GetBoundsLocalToWorld() const
    {
        return m_Transform.ToMatrix();
    }

    Math::Geometry::AABB PrimitiveSceneProxy::ResolveWorldBounds() const
    {
        if (m_PrimitiveComponent == nullptr)
        {
            return Math::Geometry::AABB{};
        }

        return Math::Geometry::Transform(
            m_PrimitiveComponent->GetBoundingBox(),
            GetBoundsLocalToWorld());
    }

    void PrimitiveSceneProxy::GetMeshBatches(
        MeshBatchCollector& collector,
        const MeshCollectContext& context) const
    {
        (void)context;
        if (m_VertexBuffer == nullptr || m_VertexInputLayout == nullptr || m_MaterialProxy == nullptr)
        {
            return;
        }

        MeshBatch batch;
        batch.m_VertexBuffer = m_VertexBuffer;
        batch.m_VertexInputLayout = m_VertexInputLayout;
        batch.m_IndexBuffer = m_IndexBuffer;
        batch.m_MaterialProxy = m_MaterialProxy;
        batch.m_LocalToWorld = m_Transform.ToMatrix();
        batch.m_CastShadow = m_CastShadow;
        batch.m_BoundingBox = ResolveWorldBounds();
        batch.m_Proxy = this;
        collector.AddMesh(batch);
    }
}
