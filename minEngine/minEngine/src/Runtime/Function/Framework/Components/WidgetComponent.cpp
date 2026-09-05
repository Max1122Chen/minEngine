#include "WidgetComponent.h"

#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
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
        m_ComputedRect.TopLeft = Vector2(0.0f, 0.0f);
        m_ComputedRect.Size = m_Size;
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

    void WidgetComponent::SetSize(const Vector2& size)
    {
        if (m_Size == size)
        {
            return;
        }
        m_Size = size;
        MarkRenderStateDirty();
    }

    void WidgetComponent::SetAnchorMin(const Vector2& anchorMin)
    {
        if (m_AnchorMin == anchorMin)
        {
            return;
        }
        m_AnchorMin = anchorMin;
        m_AnchorPreset = EUIAnchorPreset::Custom;
        MarkRenderStateDirty();
    }

    void WidgetComponent::SetAnchorMax(const Vector2& anchorMax)
    {
        if (m_AnchorMax == anchorMax)
        {
            return;
        }
        m_AnchorMax = anchorMax;
        m_AnchorPreset = EUIAnchorPreset::Custom;
        MarkRenderStateDirty();
    }

    void WidgetComponent::SetMargin(const Vector4& margin)
    {
        if (m_Margin == margin)
        {
            return;
        }
        m_Margin = margin;
        MarkRenderStateDirty();
    }

    void WidgetComponent::SetAnchorPreset(EUIAnchorPreset preset)
    {
        if (preset == EUIAnchorPreset::Custom)
        {
            m_AnchorPreset = EUIAnchorPreset::Custom;
            return;
        }
        ApplyAnchorPreset(preset);
    }

    void WidgetComponent::ApplyAnchorPreset(EUIAnchorPreset preset)
    {
        Vector2 min(0.0f, 0.0f);
        Vector2 max(0.0f, 0.0f);

        switch (preset)
        {
        case EUIAnchorPreset::TopLeft:
            min = max = Vector2(0.0f, 0.0f);
            break;
        case EUIAnchorPreset::TopCenter:
            min = max = Vector2(0.5f, 0.0f);
            break;
        case EUIAnchorPreset::TopRight:
            min = max = Vector2(1.0f, 0.0f);
            break;
        case EUIAnchorPreset::MiddleLeft:
            min = max = Vector2(0.0f, 0.5f);
            break;
        case EUIAnchorPreset::Center:
            min = max = Vector2(0.5f, 0.5f);
            break;
        case EUIAnchorPreset::MiddleRight:
            min = max = Vector2(1.0f, 0.5f);
            break;
        case EUIAnchorPreset::BottomLeft:
            min = max = Vector2(0.0f, 1.0f);
            break;
        case EUIAnchorPreset::BottomCenter:
            min = max = Vector2(0.5f, 1.0f);
            break;
        case EUIAnchorPreset::BottomRight:
            min = max = Vector2(1.0f, 1.0f);
            break;
        case EUIAnchorPreset::TopStretch:
            min = Vector2(0.0f, 0.0f);
            max = Vector2(1.0f, 0.0f);
            break;
        case EUIAnchorPreset::MiddleStretch:
            min = Vector2(0.0f, 0.5f);
            max = Vector2(1.0f, 0.5f);
            break;
        case EUIAnchorPreset::BottomStretch:
            min = Vector2(0.0f, 1.0f);
            max = Vector2(1.0f, 1.0f);
            break;
        case EUIAnchorPreset::LeftStretch:
            min = Vector2(0.0f, 0.0f);
            max = Vector2(0.0f, 1.0f);
            break;
        case EUIAnchorPreset::CenterStretch:
            min = Vector2(0.5f, 0.0f);
            max = Vector2(0.5f, 1.0f);
            break;
        case EUIAnchorPreset::RightStretch:
            min = Vector2(1.0f, 0.0f);
            max = Vector2(1.0f, 1.0f);
            break;
        case EUIAnchorPreset::StretchAll:
            min = Vector2(0.0f, 0.0f);
            max = Vector2(1.0f, 1.0f);
            break;
        case EUIAnchorPreset::Custom:
        default:
            m_AnchorPreset = EUIAnchorPreset::Custom;
            return;
        }

        m_AnchorPreset = preset;
        m_AnchorMin = min;
        m_AnchorMax = max;
        m_Margin = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
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

    void WidgetComponent::SetComputedRect(const UIRect& rect)
    {
        const bool changed = m_ComputedRect.TopLeft != rect.TopLeft || m_ComputedRect.Size != rect.Size;
        m_ComputedRect = rect;
        if (changed)
        {
            MarkRenderStateDirty();
        }
    }

    ImageComponent* WidgetComponent::FindSiblingImage() const
    {
        GameObject* owner = GetOwner();
        if (owner == nullptr)
        {
            return nullptr;
        }

        const std::vector<std::shared_ptr<ImageComponent>> images =
            owner->GetComponentsOfType<ImageComponent>();
        if (images.empty() || !images[0])
        {
            return nullptr;
        }
        return images[0].get();
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

    void WidgetComponent::SyncMaterialParameters(ImageComponent* image)
    {
        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (!m_RuntimeMaterial || rhi == nullptr || image == nullptr)
        {
            return;
        }

        ScreenUIMaterialFactory::ApplyColorAndTexture(
            *m_RuntimeMaterial,
            image->GetColor(),
            image->GetTextureShared(),
            *rhi);
    }

    void WidgetComponent::FillSceneProxy(WidgetSceneProxy& proxy)
    {
        proxy.m_WidgetComponent = this;
        proxy.m_StableOrder = m_StableOrder;
        proxy.m_UVRect = Vector4(0.0f, 0.0f, 1.0f, 1.0f);

        // Reference-space rect; Letterbox applied in BuildScreenUIQueue.
        proxy.m_TopLeftPx = m_ComputedRect.TopLeft;
        proxy.m_SizePx = m_ComputedRect.Size;

        ImageComponent* image = FindSiblingImage();
        if (image != nullptr)
        {
            proxy.m_Color = image->GetColor();
            proxy.m_Texture = image->GetTexture();
            EnsureRuntimeMaterial();
            SyncMaterialParameters(image);
            proxy.m_Material = m_RuntimeMaterial.get();
        }
        else
        {
            proxy.m_Color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            proxy.m_Texture = nullptr;
            proxy.m_Material = nullptr;
        }

        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        if (rhi != nullptr)
        {
            SpriteQuadMesh::Get().EnsureInitialized(*rhi);
        }

        SpriteQuadMesh& quad = SpriteQuadMesh::Get();
        proxy.m_VertexBuffer = quad.GetVertexBuffer();
        proxy.m_IndexBuffer = quad.GetIndexBuffer();
        proxy.m_VertexInputLayout = quad.GetVertexInputLayout();

        proxy.m_bVisible = image != nullptr && proxy.m_Material != nullptr
            && proxy.m_Material->IsCompiledForDraw() && proxy.m_VertexBuffer != nullptr
            && proxy.m_VertexInputLayout != nullptr && proxy.m_SizePx.x > 0.0f
            && proxy.m_SizePx.y > 0.0f;
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
