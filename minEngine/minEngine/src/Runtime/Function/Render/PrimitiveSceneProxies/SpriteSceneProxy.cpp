#include "SpriteSceneProxy.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"

namespace minEngine
{
    Matrix4 SpriteSceneProxy::GetBoundsLocalToWorld() const
    {
        return m_ModelMatrix;
    }

    void SpriteSceneProxy::GetMeshBatches(
        MeshBatchCollector& collector,
        const MeshCollectContext& context) const
    {
        (void)context;
        if (m_Texture == nullptr || m_VertexBuffer == nullptr || m_VertexInputLayout == nullptr || m_MaterialProxy == nullptr)
        {
            return;
        }

        MeshBatch batch;
        batch.m_VertexBuffer = m_VertexBuffer;
        batch.m_VertexInputLayout = m_VertexInputLayout;
        batch.m_IndexBuffer = m_IndexBuffer;
        batch.m_MaterialProxy = m_MaterialProxy;
        batch.m_LocalToWorld = m_ModelMatrix;
        batch.m_CastShadow = false;
        batch.m_bForceTranslucentPass = m_bNeedsTranslucentPass;
        batch.m_BoundingBox = ResolveWorldBounds();
        batch.m_Proxy = this;
        collector.AddMesh(batch);
    }
}
