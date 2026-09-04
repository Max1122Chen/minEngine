#include "WidgetComponent.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/SceneProxies/WidgetSceneProxy.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUIMaterialFactory.h"
#include "Runtime/Function/Render/Sprite/SpriteQuadMesh.h"

namespace minEngine
{
    WidgetComponent::WidgetComponent()
    {
        MarkRenderStateDirty();
    }

    WidgetComponent::~WidgetComponent()
    {
        if (!m_WidgetSceneProxy)
        {
            return;
        }

        RenderScene* renderScene = SceneManager::HasInstance() ? SceneManager::Get().GetRenderScene() : nullptr;
        if (renderScene)
        {
            renderScene->RemoveWidget(this);
        }
        else if (m_WidgetSceneProxy)
        {
            m_WidgetSceneProxy->m_WidgetComponent = nullptr;
        }

        DetachSceneProxy();
    }

    void WidgetComponent::SetTexture(const std::shared_ptr<Texture2D>& texture)
    {
        if (m_Texture == texture)
        {
            return;
        }
        m_Texture = texture;
        MarkRenderStateDirty();
    }

    void WidgetComponent::SetColor(const Vector4& color)
    {
        if (m_Color == color)
        {
            return;
        }
        m_Color = color;
        MarkRenderStateDirty();
    }

    void WidgetComponent::SetSize(const Vector2& size)
    {
        if (m_Size == size)
        {
            return;
        }
        m_Size = size;
        MarkRenderStateDirty();
    }

    void WidgetComponent::SetStableOrder(uint32_t order)
    {
        if (m_StableOrder == order)
        {
            return;
        }
        m_StableOrder = order;
        MarkRenderStateDirty();
    }

    void WidgetComponent::DoEndOfFrameUpdate()
    {
        if (!m_bRenderStateDirty || !IsActive())
        {
            return;
        }

        RenderScene* renderScene = SceneManager::Get().GetRenderScene();
        if (renderScene)
        {
            renderScene->UpdateWidget(this);
        }

        m_bRenderStateDirty = false;
    }

    void WidgetComponent::ApplyActivationToSystems()
    {
        MarkRenderStateDirty();
    }

    void WidgetComponent::RemoveActivationFromSystems()
    {
        if (!m_WidgetSceneProxy)
        {
            m_bRenderStateDirty = false;
            return;
        }

        if (SceneManager::HasInstance())
        {
            RenderScene* renderScene = SceneManager::Get().GetRenderScene();
            if (renderScene)
            {
                renderScene->RemoveWidget(this);
            }
            else if (m_WidgetSceneProxy)
            {
                m_WidgetSceneProxy->m_WidgetComponent = nullptr;
                DetachSceneProxy();
            }
        }
        else
        {
            DetachSceneProxy();
        }

        m_bRenderStateDirty = false;
    }

    void WidgetComponent::EnsureRuntimeMaterial()
    {
        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (rhi == nullptr)
        {
            return;
        }

        if (m_RuntimeMaterial && m_RuntimeMaterial->IsCompiledForDraw())
        {
            return;
        }

        m_RuntimeMaterial = ScreenUIMaterialFactory::CreateInstance(*rhi);
    }

    void WidgetComponent::SyncMaterialParameters()
    {
        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (!m_RuntimeMaterial || rhi == nullptr)
        {
            return;
        }

        ScreenUIMaterialFactory::ApplyColorAndTexture(*m_RuntimeMaterial, m_Color, m_Texture, *rhi);
    }

    void WidgetComponent::FillSceneProxy(WidgetSceneProxy& proxy)
    {
        proxy.m_WidgetComponent = this;
        proxy.m_Color = m_Color;
        proxy.m_SizePx = m_Size;
        proxy.m_StableOrder = m_StableOrder;
        proxy.m_UVRect = Vector4(0.0f, 0.0f, 1.0f, 1.0f);
        proxy.m_Texture = m_Texture.get();

        const Vector3 worldPos = GetWorldPosition();
        proxy.m_TopLeftPx = Vector2(worldPos.x, worldPos.y);

        EnsureRuntimeMaterial();
        SyncMaterialParameters();
        proxy.m_Material = m_RuntimeMaterial.get();

        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (rhi != nullptr)
        {
            SpriteQuadMesh::Get().EnsureInitialized(*rhi);
        }

        SpriteQuadMesh& quad = SpriteQuadMesh::Get();
        proxy.m_VertexBuffer = quad.GetVertexBuffer();
        proxy.m_IndexBuffer = quad.GetIndexBuffer();
        proxy.m_VertexInputLayout = quad.GetVertexInputLayout();

        proxy.m_bVisible = proxy.m_Material != nullptr && proxy.m_Material->IsCompiledForDraw()
            && proxy.m_VertexBuffer != nullptr && proxy.m_VertexInputLayout != nullptr
            && proxy.m_SizePx.x > 0.0f && proxy.m_SizePx.y > 0.0f;
    }

    WidgetSceneProxy* WidgetComponent::CreateSceneProxy()
    {
        WidgetSceneProxy* sceneProxy = new WidgetSceneProxy();
        FillSceneProxy(*sceneProxy);
        m_WidgetSceneProxy = sceneProxy;
        return sceneProxy;
    }

    void WidgetComponent::UpdateSceneProxy(WidgetSceneProxy& proxy)
    {
        FillSceneProxy(proxy);
    }
}
