#include "WidgetComponent.h"

#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/SceneProxies/WidgetSceneProxy.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUIMaterialFactory.h"
#include "Runtime/Function/Render/Sprite/SpriteQuadMesh.h"

namespace minEngine
{
    namespace
    {
        /**
         * Nearest canvas above a node, scanned in place.
         *
         * Same answer as CanvasComponent::FindOwningCanvas, but without building a component list:
         * the widget reconciles its canvas snapshot from inside the layout pass, which runs for
         * every widget on every tick, so this path must not allocate.
         */
        CanvasComponent* FindOwningCanvasInPlace(GameObject* node)
        {
            for (GameObject* current = node; current != nullptr; current = current->GetParent())
            {
                for (const std::shared_ptr<Component>& component : current->GetAllComponents())
                {
                    if (!component || component->GetClass() == nullptr ||
                        !component->IsA(CanvasComponent::StaticClass()))
                    {
                        continue;
                    }

                    return static_cast<CanvasComponent*>(component.get());
                }
            }

            return nullptr;
        }

        /** Canvas state a widget drew under when its proxy was last filled. */
        struct CanvasSnapshot
        {
            bool bHasActiveCanvas = false;
            EUICanvasScaleMode ScaleMode = EUICanvasScaleMode::Letterbox;
            Vector2 ReferenceResolution{ 0.0f, 0.0f };
            int32_t SortOrder = 0;
        };

        CanvasSnapshot CaptureCanvasSnapshot(GameObject* owner)
        {
            CanvasSnapshot snapshot;
            const CanvasComponent* canvas = FindOwningCanvasInPlace(owner);
            snapshot.bHasActiveCanvas = canvas != nullptr && canvas->IsActive();
            if (canvas != nullptr)
            {
                snapshot.ScaleMode = canvas->GetScaleMode();
                snapshot.ReferenceResolution = canvas->GetReferenceResolution();
                snapshot.SortOrder = canvas->GetSortOrder();
            }
            return snapshot;
        }

        /** Whether the canvas context moved away from what the proxy published. */
        bool CanvasSnapshotDiffers(const WidgetSceneProxy& proxy, const CanvasSnapshot& canvas)
        {
            if (proxy.m_bHasActiveCanvas != canvas.bHasActiveCanvas)
            {
                return true;
            }

            if (!canvas.bHasActiveCanvas)
            {
                return false;
            }

            return proxy.m_CanvasScaleMode != canvas.ScaleMode ||
                proxy.m_CanvasReferenceResolution != canvas.ReferenceResolution ||
                proxy.m_CanvasSortOrder != canvas.SortOrder;
        }
    }

    WidgetComponent::WidgetComponent()
    {
        m_ComputedRect.TopLeft = Vector2(0.0f, 0.0f);
        m_ComputedRect.Size = m_Size;
        MarkRenderStateDirty();
    }

    void WidgetComponent::NotifyPointerEnter(const UIPointerEvent& event)
    {
        m_OnPointerEnter.Broadcast(event);
    }

    void WidgetComponent::NotifyPointerLeave(const UIPointerEvent& event)
    {
        m_OnPointerLeave.Broadcast(event);
    }

    void WidgetComponent::NotifyPointerDown(const UIPointerEvent& event)
    {
        m_OnPointerDown.Broadcast(event);
    }

    void WidgetComponent::NotifyPointerUp(const UIPointerEvent& event)
    {
        m_OnPointerUp.Broadcast(event);
    }

    void WidgetComponent::NotifyPointerClick(const UIPointerEvent& event)
    {
        m_OnPointerClick.Broadcast(event);
    }

    WidgetComponent::~WidgetComponent()
    {
        if (!m_WidgetSceneProxy)
        {
            return;
        }

        // Nothing is written to the proxy here: its identity token is the key the queued removal
        // matches on, and that removal runs after this destructor.
        RenderScene* renderScene = GetOwningRenderSceneIfPresent();
        if (renderScene)
        {
            renderScene->RemoveWidget(this);
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

        // The canvas a widget sits under decides how its reference pixels reach the viewport, and
        // a canvas cannot observe that a widget read it: switching its scale mode or reference
        // resolution would otherwise leave the proxy drawing the mapping it was filled with.
        // Layout reaches every widget under an active canvas on each tick, so the comparison rides
        // along here instead of adding a per-frame walk of its own.
        const WidgetSceneProxy* proxy = GetSceneProxy();
        if (changed ||
            (proxy != nullptr && CanvasSnapshotDiffers(*proxy, CaptureCanvasSnapshot(GetOwner()))))
        {
            MarkRenderStateDirty();
        }
    }

    void WidgetComponent::DoEndOfFrameUpdate()
    {
        if ((!m_bRenderStateDirty && !m_bRenderTransformDirty) || !IsActive())
        {
            return;
        }

        RenderScene* renderScene = GetOwningRenderScene();
        if (renderScene)
        {
            if (GetSceneProxy() == nullptr)
            {
                renderScene->AddWidget(this);
            }
            else
            {
                // MVP: layout / material / mesh changes recreate (no Widget snapshot packet).
                renderScene->RecreateWidget(this);
            }
        }

        m_bRenderStateDirty = false;
        m_bRenderTransformDirty = false;
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

        // Same rule as the destructor: the proxy keeps its token, so the removal queued for it -
        // here, or by a scene teardown that still knows this component - still finds it.
        if (RenderScene* renderScene = GetOwningRenderSceneIfPresent())
        {
            renderScene->RemoveWidget(this);
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

        // A compile whose program is still created by the graphics role is finished: asking for
        // the drawable state here would replace a material that is about to be drawable with a new
        // equivalent one on every fill.
        if (m_RuntimeMaterial && m_RuntimeMaterial->HasCompiledProgram())
        {
            return;
        }

        m_RuntimeMaterial = ScreenUIMaterialFactory::CreateInstance(*rhi);
    }

    void WidgetComponent::FillSceneProxy(WidgetSceneProxy& proxy)
    {
        proxy.m_WidgetComponent = this;
        proxy.m_StableOrder = m_StableOrder;

        const CanvasSnapshot canvas = CaptureCanvasSnapshot(GetOwner());
        proxy.m_bHasActiveCanvas = canvas.bHasActiveCanvas;
        proxy.m_CanvasScaleMode = canvas.ScaleMode;
        proxy.m_CanvasReferenceResolution = canvas.ReferenceResolution;
        proxy.m_CanvasSortOrder = canvas.SortOrder;

        // Reference-space rect; Letterbox applied in BuildScreenUIQueue.
        proxy.m_TopLeftPx = m_ComputedRect.TopLeft;
        proxy.m_SizePx = m_ComputedRect.Size;

        proxy.m_Color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        proxy.m_UVRect = Vector4(0.0f, 0.0f, 1.0f, 1.0f);
        proxy.m_Texture = nullptr;
        proxy.ClearMeshResources();
        proxy.SetMaterialSource(nullptr);
        proxy.m_bLocalPixelMesh = false;
        proxy.m_bVisible = false;

        // The presentation axis gates the draw bit: an inactive or hidden widget assembles an
        // empty proxy, and so does one whose graphic produces no data. Each graphic gates on its
        // own switch inside the selection, so switching one off empties the picture rather than
        // freezing the last one.
        WidgetDrawData drawData;
        WidgetGraphicComponent* graphic =
            IsActiveAndVisible() ? ResolveDrawSource(drawData) : nullptr;
        if (graphic == nullptr)
        {
            return;
        }

        proxy.m_Color = drawData.Color;
        proxy.m_UVRect = drawData.UVRect;
        proxy.m_Texture = drawData.Texture;
        proxy.m_bLocalPixelMesh = drawData.bLocalPixelMesh;

        if (drawData.bLocalPixelMesh)
        {
            // The graphic hands the geometry over as owning handles: a text relayout replaces its
            // own buffers, while this proxy may still be waiting on the render thread to draw them.
            proxy.SetMeshResources(
                drawData.VertexBuffer,
                drawData.VertexInputLayout,
                drawData.IndexBuffer,
                drawData.IndexCount,
                drawData.VertexCount);
        }
        else
        {
            RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
            if (rhi != nullptr)
            {
                SpriteQuadMesh::Get().EnsureInitialized(*rhi);
            }

            SpriteQuadMesh& quad = SpriteQuadMesh::Get();
            proxy.SetMeshResources(
                quad.GetVertexBufferRef(),
                quad.GetVertexInputLayout(),
                quad.GetIndexBufferRef(),
                SpriteQuadMesh::IndexCount,
                SpriteQuadMesh::VertexCount);
        }

        EnsureRuntimeMaterial();
        if (m_RuntimeMaterial)
        {
            graphic->ApplyMaterialParameters(*m_RuntimeMaterial);
        }
        proxy.SetMaterialSource(m_RuntimeMaterial);

        // Compiled, not necessarily created yet: this flag is latched until something else marks
        // the widget dirty, so it must not capture a program that the graphics role creates at the
        // next frame start - which is before the frame this proxy is drawn in records.
        proxy.m_bVisible = canvas.bHasActiveCanvas
            && proxy.GetMaterialProxy() != nullptr
            && proxy.GetMaterialProxy()->HasCompiledProgram() && proxy.m_VertexBuffer != nullptr
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
