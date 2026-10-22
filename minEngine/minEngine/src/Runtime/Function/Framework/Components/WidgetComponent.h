#pragma once

#include "Core.h"
#include "Runtime/Core/Delegates/DelegateMacros.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/WidgetGraphicComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/UI/ScreenUIPointerTypes.h"
#include "Runtime/Function/UI/UITypes.h"

#include <string>

namespace minEngine
{
    class WidgetSceneProxy;

    DECLARE_MULTICAST_DELEGATE_OneParam(DOnWidgetPointer, const UIPointerEvent&);

    /**
     * ScreenUI layout node: Anchor/Margin/Size -> ComputedRect (Canvas reference pixels).
     * Visual style comes from a graphics component on the same GameObject.
     * Pointer events are dispatched by UISystem (hit surface).
     */
    ME_CLASS()
    class MINENGINE_API WidgetComponent : public SceneComponent
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
         * The graphic this widget paints from: the first one in component order that can
         * contribute. The proxy assembly and the pointer gate both come through here, so a widget
         * can never be drawn from one source and hit as another.
         */
        WidgetGraphicComponent* GetGraphic() const
        {
            GameObject* owner = GetOwner();
            if (owner == nullptr)
            {
                return nullptr;
            }

            for (const std::shared_ptr<Component>& component : owner->GetAllComponents())
            {
                WidgetGraphicComponent* graphic = AsGraphic(component);
                if (graphic != nullptr && graphic->CanContribute())
                {
                    return graphic;
                }
            }

            return nullptr;
        }

        /**
         * True when some graphic can contribute.
         * A widget whose graphics all contribute nothing produces no picture, so it is neither
         * drawn nor allowed to take a pointer.
         */
        bool HasGraphic() const { return GetGraphic() != nullptr; }

        const UIRect& GetComputedRect() const { return m_ComputedRect; }
        void SetComputedRect(const UIRect& rect);

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

        /** Cast to the graphic base without naming a concrete kind; null for any other component. */
        static WidgetGraphicComponent* AsGraphic(const std::shared_ptr<Component>& component)
        {
            if (!component || component->GetClass() == nullptr ||
                !component->GetClass()->IsA(WidgetGraphicComponent::LookupClass()))
            {
                return nullptr;
            }

            return static_cast<WidgetGraphicComponent*>(component.get());
        }

        void EnsureRuntimeMaterial();

        /**
         * The graphic this frame paints from and the data it produced, or null when there is
         * nothing to paint. A widget holding several graphics says which one it took and which ones
         * it left out, so a contested widget never silently swaps its picture.
         */
        WidgetGraphicComponent* ResolveDrawSource(WidgetDrawData& outData)
        {
            outData = WidgetDrawData{};

            GameObject* owner = GetOwner();
            if (owner == nullptr)
            {
                return nullptr;
            }

            WidgetGraphicComponent* selected = nullptr;
            size_t graphicCount = 0;
            std::string selectedName;
            std::string ignoredNames;

            for (const std::shared_ptr<Component>& component : owner->GetAllComponents())
            {
                WidgetGraphicComponent* graphic = AsGraphic(component);
                if (graphic == nullptr)
                {
                    continue;
                }

                ++graphicCount;
                if (selected == nullptr && graphic->CanContribute())
                {
                    selected = graphic;
                    selectedName = graphic->GetClass()->GetName();
                    continue;
                }

                if (!ignoredNames.empty())
                {
                    ignoredNames += ", ";
                }
                ignoredNames += graphic->GetClass()->GetName();
            }

            if (graphicCount > 1)
            {
                ME_LOG(
                    LogUI,
                    Warn,
                    "Widget '{}' on GameObject '{}' holds {} graphic components: drawing '{}', "
                    "ignoring [{}]. The first contributing one in component order wins; reorder the "
                    "components on the GameObject to choose a different one.",
                    GetName(),
                    owner->GetName(),
                    graphicCount,
                    selectedName.empty() ? std::string("none") : selectedName,
                    ignoredNames);
            }

            if (selected == nullptr || !selected->BuildDrawData(m_ComputedRect.Size, outData))
            {
                outData = WidgetDrawData{};
                return nullptr;
            }

            return selected;
        }

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
