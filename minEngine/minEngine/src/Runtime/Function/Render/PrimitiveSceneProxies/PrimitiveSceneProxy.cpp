#include "PrimitiveSceneProxy.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"

namespace minEngine
{
    void PrimitiveSceneProxy::GetMeshBatches(MeshBatchCollector& collector) const
    {
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
        if (m_PrimitiveComponent != nullptr)
        {
            batch.m_BoundingBox = m_PrimitiveComponent->GetBoundingBox();
        }
        batch.m_Proxy = this;
        collector.AddMesh(batch);
    }
}
