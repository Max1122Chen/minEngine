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
            sceneProxy.m_VertexBuffer = m_Mesh->m_VertexBuffer.get();
            sceneProxy.m_VertexInputLayout = m_Mesh->m_VertexInputLayout.get();
            sceneProxy.m_IndexBuffer = m_Mesh->m_IndexBuffer.get();
        }
        else
        {
            sceneProxy.m_VertexBuffer = nullptr;
            sceneProxy.m_VertexInputLayout = nullptr;
            sceneProxy.m_IndexBuffer = nullptr;
        }

        sceneProxy.m_MaterialProxy = m_Material ? m_Material->GetRenderProxy() : nullptr;
    }
}