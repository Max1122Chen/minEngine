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

    void StaticMeshComponent::SetMaterial(const std::shared_ptr<Material>& material)
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
        StaticMeshSceneProxy* SceneProxy = new StaticMeshSceneProxy();
        SceneProxy->m_PrimitiveComponent = this;

        assert(m_Owner);
        SceneProxy->m_Transform = m_Owner->GetTransform();  // TODO: calculate the final world transform by combining with parent components' transforms
        SceneProxy->m_CastShadow = CastShadow();


        if (m_Mesh)
        {
            SceneProxy->m_VertexBuffer = m_Mesh->m_VertexBuffer.get();
            SceneProxy->m_VertexDefinition = m_Mesh->m_VertexDefinition.get();
            SceneProxy->m_IndexBuffer = m_Mesh->m_IndexBuffer.get();
        }

        if (m_Material)
        {
            SceneProxy->m_Material = m_Material.get();
        }
        
        m_SceneProxy = SceneProxy;

        return SceneProxy;
    }
}