#include "SkeletalMeshSceneProxy.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"

namespace minEngine
{
    void SkeletalMeshSceneProxy::GetMeshBatches(
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
        batch.m_bSkinned = true;
        if (!m_BonePalette.empty())
        {
            batch.m_BonePalette = m_BonePalette.data();
            batch.m_BoneCount = static_cast<uint32_t>(m_BonePalette.size());
        }
        batch.m_Proxy = this;
        collector.AddMesh(batch);
    }
}
