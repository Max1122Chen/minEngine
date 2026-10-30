#include "SpriteSceneProxy.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"

namespace minEngine
{
    void SpriteSceneProxy::GetMeshBatches(
        MeshBatchCollector& collector,
        const MeshCollectContext& context) const
    {
        (void)context;
        if (m_Texture == nullptr || m_VertexBuffer == nullptr || m_VertexInputLayout == nullptr ||
            GetMaterialProxy() == nullptr)
        {
            return;
        }

        MeshBatch batch;
        batch.m_VertexBufferRef = m_VertexBufferRef;
        batch.m_IndexBufferRef = m_IndexBufferRef;
        batch.m_VertexBuffer = m_VertexBuffer;
        batch.m_VertexInputLayout = m_VertexInputLayout;
        batch.m_IndexBuffer = m_IndexBuffer;
        batch.m_IndexCount = m_IndexCount;
        batch.m_VertexCount = m_VertexCount;
        batch.m_MaterialProxy = GetMaterialProxy();
        batch.m_LocalToWorld = m_ModelMatrix;
        batch.m_CastShadow = false;
        batch.m_bForceTranslucentPass = m_bNeedsTranslucentPass;
        batch.m_BoundingBox = ResolveWorldBounds();
        batch.m_Proxy = this;
        collector.AddMesh(batch);
    }
}
