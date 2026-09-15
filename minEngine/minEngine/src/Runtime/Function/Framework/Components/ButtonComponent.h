#pragma once

#include "Core.h"
#include "Runtime/Core/Delegates/DelegateMacros.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Function/Framework/Components/Component.h"

#include <memory>

namespace minEngine
{
    class ImageComponent;
    class WidgetComponent;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(DOnButtonClicked);

    /**
     * ScreenUI button policy on the same GameObject as Widget (hit) + optional Image (skin).
     * Click is dispatched by UISystem; this component owns OnClicked and optional tint.
     */
    ME_CLASS()
    class ButtonComponent : public Component
    {
        ME_GENERATED_BODY()
    public:
        ButtonComponent();
        virtual ~ButtonComponent() override = default;

        virtual void Tick(float deltaTime) override;

        FOnButtonClicked& OnClicked() { return m_OnClicked; }
        const FOnButtonClicked& OnClicked() const { return m_OnClicked; }

        /** Broadcast OnClicked when interactable and a sibling Widget exists. */
        void NotifyClicked();

        bool IsInteractable() const { return m_bInteractable; }
        void SetInteractable(bool interactable);

        void SetTargetGraphic(const std::shared_ptr<ImageComponent>& image);
        ImageComponent* GetTargetGraphic() const { return m_TargetGraphic.get(); }
        const std::shared_ptr<ImageComponent>& GetTargetGraphicShared() const { return m_TargetGraphic; }

        /** Explicit TargetGraphic, else same-owner sibling Image. */
        ImageComponent* ResolveTargetGraphic() const;

        WidgetComponent* FindWidget() const;

        bool CanAcceptClick() const;

        void SetColorTintTransition(bool enabled);
        bool IsColorTintTransitionEnabled() const { return m_bColorTintTransition; }

        void SetNormalColor(const LinearColor& color);
        LinearColor GetNormalColor() const { return m_NormalColor; }

        void SetHighlightedColor(const LinearColor& color);
        LinearColor GetHighlightedColor() const { return m_HighlightedColor; }

        void SetPressedColor(const LinearColor& color);
        LinearColor GetPressedColor() const { return m_PressedColor; }

        void SetDisabledColor(const LinearColor& color);
        LinearColor GetDisabledColor() const { return m_DisabledColor; }

    protected:
        void OnActivate() override;
        void OnDeactivate() override;

    private:
        void ApplyVisualState();
        void ApplyTint(const LinearColor& color);

        DOnButtonClicked m_OnClicked;

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetTargetGraphic", Getter = "GetTargetGraphicShared"))
        std::shared_ptr<ImageComponent> m_TargetGraphic{ nullptr };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetInteractable", Getter = "IsInteractable"))
        bool m_bInteractable{ true };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetColorTintTransition", Getter = "IsColorTintTransitionEnabled"))
        bool m_bColorTintTransition{ true };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetNormalColor", Getter = "GetNormalColor"))
        LinearColor m_NormalColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetHighlightedColor", Getter = "GetHighlightedColor"))
        LinearColor m_HighlightedColor{ 0.9f, 0.9f, 0.9f, 1.0f };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetPressedColor", Getter = "GetPressedColor"))
        LinearColor m_PressedColor{ 0.7f, 0.7f, 0.7f, 1.0f };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetDisabledColor", Getter = "GetDisabledColor"))
        LinearColor m_DisabledColor{ 0.5f, 0.5f, 0.5f, 0.5f };
    };
}

#include "Generated/Reflection/ButtonComponent.gen.h"