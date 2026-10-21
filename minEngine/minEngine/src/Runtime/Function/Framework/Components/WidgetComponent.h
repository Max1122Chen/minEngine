#pragma once

#include "Core.h"
#include "Runtime/Core/Delegates/DelegateMacros.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/UI/ScreenUIPointerTypes.h"
#include "Runtime/Function/UI/UITypes.h"

namespace minEngine
{
    class Material;
    class WidgetSceneProxy;
    class ImageComponent;
    class TextComponent;

    DECLARE_MULTICAST_DELEGATE_OneParam(DOnWidgetPointer, const UIPointerEvent&);

    /**
     * ScreenUI layout node: Anchor/Margin/Size â†?ComputedRect (Canvas reference pixels).
     * Visual style comes from a sibling ImageComponent.
     * Pointer events are dispatched by UISystem (hit surface).
     */
    ME_CLASS()
    class WidgetComponent : public SceneComponent
    {
        ME_GENERATED_BODY()
    public:
        WidgetComponent();
        virtual ~WidgetComponent() override;

        DOnWidgetPointer& OnPointerEnter() { return m_OnPointerEnter; }
        const DOnWidgetPointer& OnPointerEnter() const { return m_OnPointerEnter; }
        DOnWidgetPointer& OnPointerLeave() { return m_OnPointerLeave; }
        const DOnWidgetPointer& OnPointerLeave() const { return m_OnPointerLeave; }
        DOnWidgetPointer& OnPointerDown() { return m_OnPointerDown; }
        const DOnWidgetPointer& OnPointerDown() const { return m_OnPointerDown; }
        DOnWidgetPointer& OnPointerUp() { return m_OnPointerUp; }
        const DOnWidgetPointer& OnPointerUp() const { return m_OnPointerUp; }
        DOnWidgetPointer& OnPointerClick() { return m_OnPointerClick; }
        const DOnWidgetPointer& OnPointerClick() const { return m_OnPointerClick; }

        void NotifyPointerEnter(const UIPointerEvent& event);
        void NotifyPointerLeave(const UIPointerEvent& event);
        void NotifyPointerDown(const UIPointerEvent& event);
        void NotifyPointerUp(const UIPointerEvent& event);
        void NotifyPointerClick(const UIPointerEvent& event);

        void SetSize(const Vector2& size);
        Vector2 GetSize() const { return m_Size; }

        void SetAnchorMin(const Vector2& anchorMin);
        Vector2 GetAnchorMin() const { return m_AnchorMin; }

        void SetAnchorMax(const Vector2& anchorMax);
        Vector2 GetAnchorMax() const { return m_AnchorMax; }

        /** Margin as (Left, Top, Right, Bottom) in reference pixels. */
        void SetMargin(const Vector4& margin);
        Vector4 GetMargin() const { return m_Margin; }

        void SetAnchorPreset(EUIAnchorPreset preset);
        EUIAnchorPreset GetAnchorPreset() const { return m_AnchorPreset; }

        /** Writes AnchorMin/Max from preset; clears Margin; keeps Size. */
        void ApplyAnchorPreset(EUIAnchorPreset preset);

        void SetStableOrder(uint32_t order);
        uint32_t GetStableOrder() const { return m_StableOrder; }

        /** When false, skipped by ScreenUI hit-test (still may draw). Default true. */
        void SetHitTestVisible(bool visible) { m_bHitTestVisible = visible; }
        bool IsHitTestVisible() const { return m_bHitTestVisible; }

        /**
         * True when a sibling image or text source currently contributes content.
         * A widget whose sources all contribute nothing produces no picture, so it is neither
         * drawn nor allowed to take a pointer.
         */
        bool HasContentContribution() const;

        const UIRect& GetComputedRect() const { return m_ComputedRect; }
        void SetComputedRect(const UIRect& rect);

        ImageComponent* FindSiblingImage() const;
        TextComponent* FindSiblingText() const;

        virtual void DoEndOfFrameUpdate() override;

        WidgetSceneProxy* CreateSceneProxy();
        WidgetSceneProxy* GetSceneProxy() const { return m_WidgetSceneProxy; }
        void DetachSceneProxy() { m_WidgetSceneProxy = nullptr; }

        void UpdateSceneProxy(WidgetSceneProxy& proxy);

    protected:
        void ApplyActivationToSystems() override;
        void RemoveActivationFromSystems() override;

    private:
        friend class Testing::TestAccess<WidgetComponent>;

        void EnsureRuntimeMaterial();
        void SyncMaterialParameters(ImageComponent* image);
        void FillSceneProxy(WidgetSceneProxy& proxy);

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetAnchorPreset", Getter = "GetAnchorPreset"))
        EUIAnchorPreset m_AnchorPreset{ EUIAnchorPreset::TopLeft };

        ME_PROPERTY()
        Vector2 m_AnchorMin{ 0.0f, 0.0f };

        ME_PROPERTY()
        Vector2 m_AnchorMax{ 0.0f, 0.0f };

        ME_PROPERTY()
        Vector4 m_Margin{ 0.0f, 0.0f, 0.0f, 0.0f };

        /** Fixed size when anchors form a point; ignored when stretching. */
        ME_PROPERTY()
        Vector2 m_Size{ 100.0f, 100.0f };

        ME_PROPERTY()
        uint32_t m_StableOrder = 0;

        bool m_bHitTestVisible = true;

        UIRect m_ComputedRect{};

        std::shared_ptr<Material> m_RuntimeMaterial{ nullptr };
        WidgetSceneProxy* m_WidgetSceneProxy = nullptr;

        DOnWidgetPointer m_OnPointerEnter;
        DOnWidgetPointer m_OnPointerLeave;
        DOnWidgetPointer m_OnPointerDown;
        DOnWidgetPointer m_OnPointerUp;
        DOnWidgetPointer m_OnPointerClick;
    };
}

#include "Generated/Reflection/WidgetComponent.gen.h"
