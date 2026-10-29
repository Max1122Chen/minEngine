#include "SpriteComponent.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/SpriteSceneProxy.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteMaterialFactory.h"
#include "Runtime/Function/Render/Sprite/SpriteQuadMesh.h"
#include "Runtime/Function/Render/Sprite/SpriteTranslucency.h"
#include "Runtime/Core/Math/Geometry/AABB.h"

#include <cmath>

namespace minEngine
{
    SpriteComponent::SpriteComponent()
    {
        m_CastShadow = false;
    }

    void SpriteComponent::SetSprite(const std::shared_ptr<Sprite>& sprite)
    {
        if (m_Sprite == sprite)
        {
            return;
        }

        m_Sprite = sprite;
        MarkRenderStateDirty();
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

    std::shared_ptr<Texture2D> SpriteComponent::ResolveDrawTextureShared() const
    {
        if (m_Sprite)
        {
            return m_Sprite->GetRuntimeTextureShared();
        }

        return m_Texture;
    }

    Vector4 SpriteComponent::ResolveDrawUVRect() const
    {
        if (m_Sprite)
        {
            return m_Sprite->GetUVRectNormalized();
        }

        return m_UVRect;
    }

    bool SpriteComponent::IsUVRectDegenerate(const Vector4& uvRect)
    {
        constexpr float kEps = 1.0e-6f;
        return std::fabs(uvRect.z - uvRect.x) < kEps || std::fabs(uvRect.w - uvRect.y) < kEps;
    }

    Math::Geometry::AABB SpriteComponent::GetBoundingBox() const
    {
        if (m_Size.x <= 0.0f || m_Size.y <= 0.0f)
        {
            return Math::Geometry::AABB();
        }

        // Component space, like every other primitive: the size is the authored one, and the world
        // transform is applied once by whoever needs the world box.
        const Vector3 halfExtent(m_Size.x * 0.5f, m_Size.y * 0.5f, 0.01f);
        return Math::Geometry::AABB(-halfExtent, halfExtent);
    }

    void SpriteComponent::EnsureRuntimeMaterial()
    {
        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (rhi == nullptr)
        {
            return;
        }

        Texture2D* drawTexture = ResolveDrawTexture();
        const bool needsTranslucent = ComputeSpriteNeedsTranslucentPass(m_Color, drawTexture, false);
        // Compiled is enough: a program the graphics role still has to create becomes drawable
        // before the next frame records, so recreating the material for it would only lose the
        // parameters applied to this one.
        if (m_RuntimeMaterial && m_bRuntimeMaterialTranslucent == needsTranslucent &&
            m_RuntimeMaterial->HasCompiledProgram())
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

        SpriteMaterialFactory::ApplyColorTextureAndUVRect(
            *m_RuntimeMaterial,
            m_Color,
            ResolveDrawTextureShared(),
            ResolveDrawUVRect());
    }

    void SpriteComponent::FillSceneProxy(SpriteSceneProxy& proxy)
    {
        proxy.m_PrimitiveComponent = this;
        proxy.m_CastShadow = false;
        proxy.m_Color = m_Color.ToVector4();
        proxy.m_Size = m_Size;

        // This path fills the proxy without PrimitiveComponent::SyncSceneProxy, so it captures the
        // world box itself and keeps the same rule: computed here, read on the render role.
        proxy.SetWorldBounds(Math::Geometry::Transform(GetBoundingBox(), GetWorldMatrix()));

        const std::shared_ptr<Texture2D> drawTexture = ResolveDrawTextureShared();
        const Vector4 drawUv = ResolveDrawUVRect();
        proxy.m_Texture = drawTexture.get();
        proxy.m_UVRect = drawUv;

        if (drawTexture == nullptr || IsUVRectDegenerate(drawUv) || m_Size.x <= 0.0f || m_Size.y <= 0.0f)
        {
            proxy.ClearMeshResources();
            proxy.SetMaterialSource(nullptr);
            proxy.m_bNeedsTranslucentPass = false;
            proxy.m_ModelMatrix = GetWorldMatrix();
            proxy.m_Transform = GetWorldTransform();
            return;
        }

        EnsureRuntimeMaterial();
        SyncMaterialParameters();
        proxy.SetMaterialSource(m_RuntimeMaterial);

        proxy.m_bNeedsTranslucentPass =
            ComputeSpriteNeedsTranslucentPass(
                m_Color,
                drawTexture.get(),
                proxy.GetMaterialProxy() && proxy.GetMaterialProxy()->IsTranslucent());

        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (rhi != nullptr)
        {
            SpriteQuadMesh::Get().EnsureInitialized(*rhi);
        }

        // The proxy holds the quad geometry itself, so a sprite keeps drawing even if the game
        // thread drops the component's material or re-points the sprite later this frame.
        SpriteQuadMesh& quad = SpriteQuadMesh::Get();
        proxy.SetMeshResources(
            quad.GetVertexBufferRef(),
            quad.GetVertexInputLayoutRef(),
            quad.GetIndexBufferRef(),
            SpriteQuadMesh::IndexCount,
            SpriteQuadMesh::VertexCount);

        const Matrix4 worldMatrix = GetWorldMatrix();
        const Matrix4 sizeScale = glm::scale(Matrix4(1.0f), Vector3(m_Size.x, m_Size.y, 1.0f));
        proxy.m_ModelMatrix = worldMatrix * sizeScale;
        proxy.m_Transform = GetWorldTransform();
    }

    PrimitiveSceneProxy* SpriteComponent::CreateSceneProxy()
    {
        SpriteSceneProxy* sceneProxy = new SpriteSceneProxy();
        SyncSceneProxy(*sceneProxy);
        m_SceneProxy = sceneProxy;
        return sceneProxy;
    }

    void SpriteComponent::SyncSceneProxy(PrimitiveSceneProxy& proxy)
    {
        FillSceneProxy(static_cast<SpriteSceneProxy&>(proxy));
    }
}
