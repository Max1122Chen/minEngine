#include "StaticMeshComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/StaticMeshSceneProxy.h"

namespace minEngine
{

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

    PrimitiveSceneProxy* StaticMeshComponent::CreateSceneProxy()
    {
        StaticMeshSceneProxy* SceneProxy = new StaticMeshSceneProxy();

        if(m_Owner)
        {
            SceneProxy->m_Transform = m_Owner->GetTransform();
        }
        else
        {
            SceneProxy->m_Transform = m_Transform;
        }

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