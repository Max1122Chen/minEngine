#include "SpriteComponent.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/SpriteSceneProxy.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Sprite/SpriteMaterialFactory.h"
#include "Runtime/Function/Render/Sprite/SpriteQuadMesh.h"
#include "Runtime/Function/Render/Sprite/SpriteTranslucency.h"
#include "Runtime/Core/Math/Geometry/AABB.h"

namespace minEngine
{
    SpriteComponent::SpriteComponent()
    {
        m_CastShadow = false;
    }

    void SpriteComponent::SetTexture(const std::shared_ptr<Texture2D>& texture)
    {
        if (m_Texture == texture)
        {
            return;
        }
        m_Texture = texture;
        MarkRenderStateDirty();
    }

    void SpriteComponent::SetColor(const LinearColor& color)
    {
        if (m_Color == color)
        {
            return;
        }
        m_Color = color;
        MarkRenderStateDirty();
    }

    void SpriteComponent::SetSize(const Vector2& size)
    {
        if (m_Size == size)
        {
            return;
        }
        m_Size = size;
        MarkRenderStateDirty();
    }

    void SpriteComponent::SetUVRect(const Vector4& uvRect)
    {
        if (m_UVRect == uvRect)
        {
            return;
        }
        m_UVRect = uvRect;
        MarkRenderStateDirty();
    }

    Math::Geometry::AABB SpriteComponent::GetBoundingBox() const
    {
        if (m_Size.x <= 0.0f || m_Size.y <= 0.0f)
        {
            return Math::Geometry::AABB();
        }

        const Vector3 halfExtent(m_Size.x * 0.5f, m_Size.y * 0.5f, 0.01f);
        const Math::Geometry::AABB localBox(-halfExtent, halfExtent);
        return Math::Geometry::Transform(localBox, GetWorldMatrix());
    }

    void SpriteComponent::EnsureRuntimeMaterial()
    {
        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (rhi == nullptr)
        {
            return;
        }

        const bool needsTranslucent = ComputeSpriteNeedsTranslucentPass(m_Color, m_Texture.get(), false);
        if (m_RuntimeMaterial && m_bRuntimeMaterialTranslucent == needsTranslucent &&
            m_RuntimeMaterial->IsCompiledForDraw())
        {
            return;
        }

        m_RuntimeMaterial = SpriteMaterialFactory::CreateInstance(*rhi, needsTranslucent);
        m_bRuntimeMaterialTranslucent = needsTranslucent;
    }

    void SpriteComponent::SyncMaterialParameters()
    {
        if (!m_RuntimeMaterial)
        {
            return;
        }

        SpriteMaterialFactory::ApplyColorAndTexture(*m_RuntimeMaterial, m_Color, m_Texture);
    }

    void SpriteComponent::FillSceneProxy(SpriteSceneProxy& proxy)
    {
        proxy.m_PrimitiveComponent = this;
        proxy.m_CastShadow = false;
        proxy.m_Color = m_Color.ToVector4();
        proxy.m_UVRect = m_UVRect;
        proxy.m_Size = m_Size;
        proxy.m_Texture = m_Texture.get();

        EnsureRuntimeMaterial();
        SyncMaterialParameters();
        proxy.m_Material = m_RuntimeMaterial.get();

        proxy.m_bNeedsTranslucentPass =
            ComputeSpriteNeedsTranslucentPass(m_Color, m_Texture.get(), proxy.m_Material && proxy.m_Material->IsTranslucent());

        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (rhi != nullptr)
        {
            SpriteQuadMesh::Get().EnsureInitialized(*rhi);
        }

        SpriteQuadMesh& quad = SpriteQuadMesh::Get();
        proxy.m_VertexBuffer = quad.GetVertexBuffer();
        proxy.m_IndexBuffer = quad.GetIndexBuffer();
        proxy.m_VertexInputLayout = quad.GetVertexInputLayout();

        const Matrix4 worldMatrix = GetWorldMatrix();
        const Matrix4 sizeScale = glm::scale(Matrix4(1.0f), Vector3(m_Size.x, m_Size.y, 1.0f));
        proxy.m_ModelMatrix = worldMatrix * sizeScale;

        // Keep Transform for tools that still read it; model matrix is authoritative for draws.
        proxy.m_Transform = GetWorldTransform();
    }

    PrimitiveSceneProxy* SpriteComponent::CreateSceneProxy()
    {
        SpriteSceneProxy* sceneProxy = new SpriteSceneProxy();
        FillSceneProxy(*sceneProxy);
        m_SceneProxy = sceneProxy;
        return sceneProxy;
    }

    void SpriteComponent::UpdateSceneProxy(SpriteSceneProxy& proxy)
    {
        FillSceneProxy(proxy);
    }
}
