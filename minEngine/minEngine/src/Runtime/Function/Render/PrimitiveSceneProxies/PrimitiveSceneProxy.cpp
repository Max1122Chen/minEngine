#include "PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"

namespace minEngine
{
    void PrimitiveSceneProxy::SetMeshResources(
        const RHIBufferRef& vertexBuffer,
        const RHIVertexInputLayoutRef& vertexInputLayout,
        const RHIBufferRef& indexBuffer,
        uint32_t indexCount,
        uint32_t vertexCount)
    {
        m_VertexBufferRef = vertexBuffer;
        m_VertexInputLayoutRef = vertexInputLayout;
        m_IndexBufferRef = indexBuffer;

        m_VertexBuffer = m_VertexBufferRef.get();
        m_VertexInputLayout = m_VertexInputLayoutRef.get();
        m_IndexBuffer = m_IndexBufferRef.get();

        m_IndexCount = indexCount;
        m_VertexCount = vertexCount;
    }

    void PrimitiveSceneProxy::ClearMeshResources()
    {
        SetMeshResources(nullptr, nullptr, nullptr, 0, 0);
    }

    void PrimitiveSceneProxy::SetMaterialSource(const std::shared_ptr<MaterialInterface>& material)
    {
        // Resolved once, here: the only place this proxy asks the material for anything.
        MaterialRenderProxyRef resolved = material ? material->GetRenderProxyRef() : nullptr;
        if (m_MaterialSource == resolved)
        {
            return;
        }

        // An unbound proxy does not displace what it drew with before: a re-sync that clears and
        // re-binds must not drop the material an older command can still reference.
        if (m_MaterialSource)
        {
            m_PreviousMaterialSource = std::move(m_MaterialSource);
        }

        m_MaterialSource = std::move(resolved);
        // Game role: the one read of the material that draw collection used to repeat every frame.
        m_MaterialProxy = m_MaterialSource.get();
    }

    Matrix4 PrimitiveSceneProxy::GetBoundsLocalToWorld() const
    {
        return m_Transform.ToMatrix();
    }

    Math::Geometry::AABB PrimitiveSceneProxy::ResolveWorldBounds() const
    {
        // The box was transformed on the game role, where the component and its asset still exist.
        // Nothing here may reach back into the component: this runs while a queued removal has not
        // been applied yet, which is exactly when the game role may already have destroyed it.
        return m_WorldBounds;
    }

    void PrimitiveSceneProxy::GetMeshBatches(
        MeshBatchCollector& collector,
        const MeshCollectContext& context) const
    {
        (void)context;
        if (m_VertexBuffer == nullptr || m_VertexInputLayout == nullptr || GetMaterialProxy() == nullptr)
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
        batch.m_LocalToWorld = GetBoundsLocalToWorld();
        batch.m_CastShadow = m_CastShadow;
        batch.m_BoundingBox = ResolveWorldBounds();
        batch.m_Proxy = this;
        collector.AddMesh(batch);
    }
}
