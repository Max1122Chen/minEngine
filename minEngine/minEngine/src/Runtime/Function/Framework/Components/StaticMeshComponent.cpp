#include "StaticMeshComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Material.h"

namespace minEngine
{
    StaticMeshComponent::StaticMeshComponent()
    {
    }

    void StaticMeshComponent::SetMesh(const std::shared_ptr<StaticMesh>& mesh)
    {
        if(m_Mesh == mesh)
        {
            return;
        }
        m_Mesh = mesh;
        MarkRenderStateDirty();
    }

    void StaticMeshComponent::SetMaterial(const std::shared_ptr<MaterialInterface>& material)
    {
        if(m_Material == material)
        {
            return;
        }
        m_Material = material;
        MarkRenderStateDirty();
    }

    Math::Geometry::AABB StaticMeshComponent::GetBoundingBox() const
    {
        if(m_Mesh)
        {
            return m_Mesh->m_BoundingBox;
        }
        return Math::Geometry::AABB();
    }

    PrimitiveSceneProxy* StaticMeshComponent::CreateSceneProxy()
    {
        StaticMeshSceneProxy* sceneProxy = new StaticMeshSceneProxy();
        SyncSceneProxy(*sceneProxy);
        m_SceneProxy = sceneProxy;
        return sceneProxy;
    }

    void StaticMeshComponent::SyncSceneProxy(PrimitiveSceneProxy& proxy)
    {
        PrimitiveComponent::SyncSceneProxy(proxy);

        StaticMeshSceneProxy& sceneProxy = static_cast<StaticMeshSceneProxy&>(proxy);
        if (m_Mesh)
        {
            // The proxy keeps the asset's buffers alive; the component may exchange or release the
            // mesh afterwards while the proxy is still drawing on the render thread.
            uint32_t indexCount = 0;
            for (const StaticMeshSectionInfo& section : m_Mesh->m_Sections)
            {
                indexCount += section.NumIndices;
            }
            if (indexCount == 0 && m_Mesh->m_IndexBuffer)
            {
                // A mesh built directly from raw arrays has no section table; its single range is
                // the whole index buffer. Captured here, on the game thread, so submission never
                // has to ask the GPU object.
                indexCount = m_Mesh->m_IndexBuffer->GetDesc().ElementCount;
            }

            const uint32_t vertexCount =
                m_Mesh->m_VertexBuffer ? m_Mesh->m_VertexBuffer->GetDesc().ElementCount : 0;

            sceneProxy.SetMeshResources(
                m_Mesh->m_VertexBuffer,
                m_Mesh->m_VertexInputLayout,
                m_Mesh->m_IndexBuffer,
                indexCount,
                vertexCount);
        }
        else
        {
            sceneProxy.ClearMeshResources();
        }

        sceneProxy.SetMaterialSource(m_Material);
    }
}