#include "Runtime/Function/UI/UISystem.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/ButtonComponent.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/DragSourceComponent.h"
#include "Runtime/Function/Framework/Components/DropTargetComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Input/InputKeys.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"
#include "Runtime/Function/UI/Font/FontGlyphAtlasCache.h"

#include <cmath>

namespace minEngine
{
    UISystem* UISystem::s_Instance = nullptr;

    UISystem::UISystem() = default;

    UISystem::~UISystem()
    {
        Shutdown();
    }

    void UISystem::SetInstance(UISystem* instance)
    {
        s_Instance = instance;
    }

    UISystem& UISystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "UISystem is not initialized");
        return *s_Instance;
    }

    bool UISystem::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void UISystem::Initialize()
    {
        if (m_Initialized)
        {
            return;
        }

        m_PointerState.Reset();
        m_ViewportContext.Reset();
        m_DragSession.Reset();
        m_bPointerRoutingEnabled = false;
        m_PIEScene = nullptr;
        m_Initialized = true;
        ME_LOG(LogUI, Info, "UISystem Initialized");
    }

    void UISystem::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        ClearDragSession();
        m_PointerState.Reset();
        m_ViewportContext.Reset();
        m_bPointerRoutingEnabled = false;
        m_PIEScene = nullptr;
        FontGlyphAtlasCache::Get().Clear();
        m_Initialized = false;
        ME_LOG(LogUI, Info, "UISystem Shutdown");
    }

    void UISystem::Tick(float deltaTime)
    {
        (void)deltaTime;
        if (!m_Initialized)
        {
            return;
        }

        m_PointerState.bClickThisFrame = false;
        m_PointerState.Clicked = nullptr;
        m_PointerState.ClickButton = EUIPointerButton::None;
        m_PointerState.bButtonOnClickedThisFrame = false;

        if (!m_bPointerRoutingEnabled)
        {
            return;
        }

        UpdatePointerFromInput(ResolvePointerScene());
    }

    void UISystem::SetPointerViewportContext(const ScreenUIPointerViewportContext& context)
    {
        m_ViewportContext = context;
    }

    void UISystem::SetDefaultDragThresholdPx(float thresholdPx)
    {
        if (thresholdPx > 0.0f && std::isfinite(thresholdPx))
        {
            m_DefaultDragThresholdPx = thresholdPx;
        }
    }

    bool UISystem::ShouldBlockWorldPointer() const
    {
        if (!m_Initialized || !m_bPointerRoutingEnabled)
        {
            return false;
        }

        return m_PointerState.Hovered != nullptr || m_PointerState.Pressed != nullptr
            || m_DragSession.Phase != EUIDragPhase::None;
    }

    void UISystem::PollPointer()
    {
        if (!m_Initialized || !m_bPointerRoutingEnabled)
        {
            return;
        }

        UpdatePointerFromInput(ResolvePointerScene());
    }

    Scene* UISystem::ResolvePointerScene() const
    {
        if (m_PIEScene != nullptr)
        {
            return m_PIEScene;
        }

        if (SceneManager::HasInstance())
        {
            const std::shared_ptr<Scene> active = SceneManager::Get().GetCurrentActiveScene();
            return active.get();
        }

        return nullptr;
    }

    ScreenUIHitResult UISystem::HitTestAt(const ScreenUIHitQuery& query) const
    {
        return m_HitTester.HitTestAt(query);
    }

    ScreenUIHitResult UISystem::HitTestAtWindowMouse(Scene* scene) const
    {
        ScreenUIHitResult result;
        result.Reset();

        if (scene == nullptr || !m_ViewportContext.bValid)
        {
            return result;
        }

        if (m_ViewportContext.ImageSize.x <= 0.0f || m_ViewportContext.ImageSize.y <= 0.0f)
        {
            return result;
        }

        if (!InputSystem::HasInstance())
        {
            return result;
        }

        const Vector2 windowMouse = InputSystem::GetMousePosition();
        ScreenUIHitQuery query;
        query.ViewportPoint = windowMouse - m_ViewportContext.ImageMin;
        query.ViewportWidth = m_ViewportContext.ImageSize.x;
        query.ViewportHeight = m_ViewportContext.ImageSize.y;
        query.Scene = scene;
        return HitTestAt(query);
    }

    void UISystem::SetPointerRoutingEnabled(bool enabled)
    {
        m_bPointerRoutingEnabled = enabled;
        if (!enabled)
        {
            CancelDragSession();
            m_PointerState.Reset();
        }
    }

    void UISystem::OnBeginPIE(Scene* pieScene)
    {
        m_PIEScene = pieScene;
        SetPointerRoutingEnabled(true);
    }

    void UISystem::OnEndPIE(Scene* pieScene)
    {
        (void)pieScene;
        m_PIEScene = nullptr;
        m_ViewportContext.Reset();
        SetPointerRoutingEnabled(false);
    }

    void UISystem::CancelDragIfSource(DragSourceComponent* source)
    {
        if (source == nullptr || m_DragSession.Source != source)
        {
            return;
        }
        CancelDragSession();
    }

    void UISystem::SampleModifiers()
    {
        UIKeyModifiers mods{};
        if (InputSystem::HasInstance())
        {
            mods.bLeftShift = InputSystem::KeyDown(InputKeys::Key_LeftShift);
            mods.bRightShift = InputSystem::KeyDown(InputKeys::Key_RightShift);
            mods.bLeftCtrl = InputSystem::KeyDown(InputKeys::Key_LeftControl);
            mods.bRightCtrl = InputSystem::KeyDown(InputKeys::Key_RightControl);
            mods.bLeftAlt = InputSystem::KeyDown(InputKeys::Key_LeftAlt);
            mods.bRightAlt = InputSystem::KeyDown(InputKeys::Key_RightAlt);
            mods.bShift = InputSystem::KeyDown(InputKeys::Key_Shift);
            mods.bCtrl = InputSystem::KeyDown(InputKeys::Key_Control);
            mods.bAlt = InputSystem::KeyDown(InputKeys::Key_Alt);
            if (!mods.bShift)
            {
                mods.bShift = mods.bLeftShift || mods.bRightShift;
            }
            if (!mods.bCtrl)
            {
                mods.bCtrl = mods.bLeftCtrl || mods.bRightCtrl;
            }
            if (!mods.bAlt)
            {
                mods.bAlt = mods.bLeftAlt || mods.bRightAlt;
            }
        }
        m_PointerState.Modifiers = mods;
    }

    UIPointerEvent UISystem::MakePointerEvent(
        EUIPointerButton button,
        WidgetComponent* widget,
        const Vector2& referencePx,
        int32_t clickCount) const
    {
        UIPointerEvent event{};
        event.Button = button;
        event.Modifiers = m_PointerState.Modifiers;
        event.ReferencePx = referencePx;
        event.ClickCount = clickCount;
        event.Widget = widget;
        return event;
    }

    void UISystem::UpdateHover(WidgetComponent* newHovered, const Vector2& referencePx)
    {
        WidgetComponent* oldHovered = m_PointerState.Hovered;
        if (oldHovered == newHovered)
        {
            return;
        }

        if (oldHovered != nullptr)
        {
            oldHovered->NotifyPointerLeave(MakePointerEvent(EUIPointerButton::None, oldHovered, referencePx, 0));
        }

        m_PointerState.Hovered = newHovered;
        if (newHovered != nullptr)
        {
            newHovered->NotifyPointerEnter(MakePointerEvent(EUIPointerButton::None, newHovered, referencePx, 0));
        }
    }

    int32_t UISystem::ResolveClickCount(WidgetComponent* widget, EUIPointerButton button)
    {
        const auto now = std::chrono::steady_clock::now();
        int32_t clickCount = 1;
        if (widget != nullptr
            && widget == m_LastClickWidget
            && button == m_LastClickButton
            && m_LastClickWidget != nullptr)
        {
            const float elapsed = std::chrono::duration<float>(now - m_LastClickTime).count();
            if (elapsed <= m_DoubleClickSeconds)
            {
                clickCount = 2;
            }
        }

        m_LastClickWidget = widget;
        m_LastClickButton = button;
        m_LastClickTime = now;
        return clickCount;
    }

    void UISystem::TryDispatchButtonClick(WidgetComponent* pressedWidget)
    {
        if (pressedWidget == nullptr)
        {
            return;
        }

        UIPointerEvent event = MakePointerEvent(EUIPointerButton::Left, pressedWidget, Vector2(0.0f, 0.0f), 1);
        TryDispatchPointerClick(pressedWidget, event);
    }

    void UISystem::TryDispatchPointerClick(WidgetComponent* widget, const UIPointerEvent& event)
    {
        if (widget == nullptr)
        {
            return;
        }

        widget->NotifyPointerClick(event);

        GameObject* owner = widget->GetOwner();
        if (owner == nullptr)
        {
            return;
        }

        const std::vector<std::shared_ptr<ButtonComponent>> buttons =
            owner->GetComponentsOfType<ButtonComponent>();
        for (const std::shared_ptr<ButtonComponent>& button : buttons)
        {
            if (!button || !button->CanAcceptClick())
            {
                continue;
            }

            button->NotifyPointerClick(event);
            if (event.Button == EUIPointerButton::Left)
            {
                m_PointerState.bButtonOnClickedThisFrame = true;
            }
        }
    }

    void UISystem::SyncLegacyPointerFlags()
    {
        m_PointerState.Pressed = m_PointerState.Left.Pressed;
        m_PointerState.bPointerDown =
            m_PointerState.Left.bDown || m_PointerState.Right.bDown || m_PointerState.Middle.bDown;
    }

    bool UISystem::TryUnmapReferencePx(
        CanvasComponent& canvas,
        const Vector2& viewportPoint,
        float viewportWidth,
        float viewportHeight,
        Vector2& outReferencePx) const
    {
        const Vector2 ref = canvas.GetReferenceResolution();
        const ScreenUICoords::LetterboxMapping mapping = ScreenUICoords::MakeLetterboxMapping(
            ref.x,
            ref.y,
            viewportWidth,
            viewportHeight);
        return mapping.TryUnmapPoint(viewportPoint, ref.x, ref.y, outReferencePx);
    }

    DragSourceComponent* UISystem::FindDragSource(WidgetComponent* widget) const
    {
        if (widget == nullptr)
        {
            return nullptr;
        }
        GameObject* owner = widget->GetOwner();
        if (owner == nullptr)
        {
            return nullptr;
        }

        const std::vector<std::shared_ptr<DragSourceComponent>> sources =
            owner->GetComponentsOfType<DragSourceComponent>();
        for (const std::shared_ptr<DragSourceComponent>& source : sources)
        {
            if (source && source->CanStartDrag())
            {
                return source.get();
            }
        }
        return nullptr;
    }

    DropTargetComponent* UISystem::FindDropTarget(WidgetComponent* widget) const
    {
        if (widget == nullptr)
        {
            return nullptr;
        }
        GameObject* owner = widget->GetOwner();
        if (owner == nullptr)
        {
            return nullptr;
        }

        const std::vector<std::shared_ptr<DropTargetComponent>> targets =
            owner->GetComponentsOfType<DropTargetComponent>();
        for (const std::shared_ptr<DropTargetComponent>& target : targets)
        {
            if (target && target->IsActive())
            {
                return target.get();
            }
        }
        return nullptr;
    }

    void UISystem::BeginPendingDrag(
        WidgetComponent* widget,
        const Vector2& referencePx,
        EUIPointerButton button)
    {
        if (m_DragSession.Phase != EUIDragPhase::None)
        {
            return;
        }

        DragSourceComponent* source = FindDragSource(widget);
        if (source == nullptr)
        {
            return;
        }

        m_DragSession.Reset();
        m_DragSession.Phase = EUIDragPhase::Pending;
        m_DragSession.SourceWidget = widget;
        m_DragSession.Source = source;
        m_DragSession.SourceCanvas =
            CanvasComponent::FindOwningCanvas(widget != nullptr ? widget->GetOwner() : nullptr);
        m_DragSession.PressReferencePx = referencePx;
        m_DragSession.CurrentReferencePx = referencePx;
        m_DragSession.PointerDeltaPx = Vector2(0.0f, 0.0f);
        m_DragSession.bSuppressClick = false;
        m_DragSession.InitiatingButton = button;
    }

    void UISystem::TryPromotePendingDrag()
    {
        if (m_DragSession.Phase != EUIDragPhase::Pending || m_DragSession.Source == nullptr)
        {
            return;
        }

        float threshold = m_DragSession.Source->GetDragThresholdPx();
        if (threshold <= 0.0f || !std::isfinite(threshold))
        {
            threshold = m_DefaultDragThresholdPx;
        }

        const float dx = m_DragSession.PointerDeltaPx.x;
        const float dy = m_DragSession.PointerDeltaPx.y;
        if ((dx * dx + dy * dy) < (threshold * threshold))
        {
            return;
        }

        m_DragSession.Phase = EUIDragPhase::Dragging;
        m_DragSession.bSuppressClick = true;
        m_DragSession.Source->NotifyDragBegin();
    }

    void UISystem::UpdateDragging(WidgetComponent* hoveredWidget, const Vector2& referencePx)
    {
        if (m_DragSession.Phase != EUIDragPhase::Dragging || m_DragSession.Source == nullptr)
        {
            return;
        }

        m_DragSession.CurrentReferencePx = referencePx;
        m_DragSession.PointerDeltaPx = referencePx - m_DragSession.PressReferencePx;
        m_DragSession.HoveredTarget = FindDropTarget(hoveredWidget);
        m_DragSession.Source->NotifyDrag();
    }

    void UISystem::FinishDragging(WidgetComponent* hoveredWidget, bool bReleased)
    {
        if (m_DragSession.Phase != EUIDragPhase::Dragging || m_DragSession.Source == nullptr)
        {
            ClearDragSession();
            return;
        }

        DropTargetComponent* target = FindDropTarget(hoveredWidget);
        const bool bAccept =
            bReleased && target != nullptr && target->CanAcceptDrop(m_DragSession);

        m_DragSession.Source->SetLastEndSuccess(bAccept);
        if (bAccept)
        {
            target->NotifyDrop();
            m_DragSession.Source->NotifyDragEnd();
        }
        else
        {
            m_DragSession.Source->NotifyDragCancel();
            m_DragSession.Source->NotifyDragEnd();
        }

        ClearDragSession();
    }

    void UISystem::ClearDragSession()
    {
        m_DragSession.Reset();
    }

    void UISystem::CancelDragSession()
    {
        if (m_DragSession.Phase == EUIDragPhase::Dragging && m_DragSession.Source != nullptr)
        {
            m_DragSession.Source->SetLastEndSuccess(false);
            m_DragSession.Source->NotifyDragCancel();
            m_DragSession.Source->NotifyDragEnd();
        }
        ClearDragSession();
    }

    void UISystem::UpdatePointerFromInput(Scene* scene)
    {
        if (!InputSystem::HasInstance())
        {
            return;
        }

        const ScreenUIHitResult hit = HitTestAtWindowMouse(scene);

        Vector2 referencePx = hit.PointInReferenceSpace;
        bool bHaveReferencePx = hit.bHit;
        if (!bHaveReferencePx && m_DragSession.SourceCanvas != nullptr && m_ViewportContext.bValid)
        {
            const Vector2 windowMouse = InputSystem::GetMousePosition();
            const Vector2 viewportPoint = windowMouse - m_ViewportContext.ImageMin;
            bHaveReferencePx = TryUnmapReferencePx(
                *m_DragSession.SourceCanvas,
                viewportPoint,
                m_ViewportContext.ImageSize.x,
                m_ViewportContext.ImageSize.y,
                referencePx);
        }

        SampleModifiers();
        UpdateHover(hit.bHit ? hit.Widget : nullptr, referencePx);

        const struct
        {
            EUIPointerButton Button;
            const InputKey* Key;
        } tracked[] = {
            { EUIPointerButton::Left, &InputKeys::Mouse_Left },
            { EUIPointerButton::Right, &InputKeys::Mouse_Right },
            { EUIPointerButton::Middle, &InputKeys::Mouse_Middle },
        };

        for (const auto& entry : tracked)
        {
            ProcessButtonEdges(
                entry.Button,
                InputSystem::MouseButtonPressed(*entry.Key),
                InputSystem::MouseButtonDown(*entry.Key),
                InputSystem::MouseButtonReleased(*entry.Key),
                hit.bHit ? hit.Widget : nullptr,
                referencePx,
                bHaveReferencePx);
        }

        SyncLegacyPointerFlags();
        (void)scene;
    }

    void UISystem::UpdatePointerFrame(
        Scene* scene,
        const ScreenUIHitResult& hit,
        const Vector2& referencePx,
        bool bHaveReferencePx,
        bool buttonPressed,
        bool buttonDown,
        bool buttonReleased)
    {
        UpdatePointerFrame(
            scene,
            hit,
            referencePx,
            bHaveReferencePx,
            EUIPointerButton::Left,
            buttonPressed,
            buttonDown,
            buttonReleased);
    }

    void UISystem::UpdatePointerFrame(
        Scene* scene,
        const ScreenUIHitResult& hit,
        const Vector2& referencePx,
        bool bHaveReferencePx,
        EUIPointerButton button,
        bool buttonPressed,
        bool buttonDown,
        bool buttonReleased)
    {
        (void)scene;
        SampleModifiers();
        UpdateHover(hit.bHit ? hit.Widget : nullptr, referencePx);
        ProcessButtonEdges(
            button,
            buttonPressed,
            buttonDown,
            buttonReleased,
            hit.bHit ? hit.Widget : nullptr,
            referencePx,
            bHaveReferencePx);
        SyncLegacyPointerFlags();
    }

    void UISystem::ProcessButtonEdges(
        EUIPointerButton button,
        bool buttonPressed,
        bool buttonDown,
        bool buttonReleased,
        WidgetComponent* hitWidget,
        const Vector2& referencePx,
        bool bHaveReferencePx)
    {
        if (button == EUIPointerButton::None)
        {
            return;
        }

        ScreenUIPerButtonPointerState& buttonState = m_PointerState.GetButtonState(button);

        if (buttonPressed)
        {
            buttonState.bDown = true;
            buttonState.Pressed = hitWidget;
            if (hitWidget != nullptr)
            {
                hitWidget->NotifyPointerDown(MakePointerEvent(button, hitWidget, referencePx, 0));
            }
            if (hitWidget != nullptr && bHaveReferencePx)
            {
                BeginPendingDrag(hitWidget, referencePx, button);
            }
        }

        if (buttonReleased)
        {
            const bool bIsInitiator = m_DragSession.InitiatingButton == button;
            const bool bWasDragging =
                bIsInitiator && m_DragSession.Phase == EUIDragPhase::Dragging;
            const bool bSuppressClick =
                bIsInitiator && m_DragSession.bSuppressClick;

            if (bWasDragging)
            {
                if (bHaveReferencePx)
                {
                    m_DragSession.CurrentReferencePx = referencePx;
                    m_DragSession.PointerDeltaPx = referencePx - m_DragSession.PressReferencePx;
                }
                FinishDragging(m_PointerState.Hovered, true);
            }
            else if (bIsInitiator && m_DragSession.Phase == EUIDragPhase::Pending)
            {
                ClearDragSession();
            }

            WidgetComponent* pressedWidget = buttonState.Pressed;
            if (pressedWidget != nullptr)
            {
                pressedWidget->NotifyPointerUp(MakePointerEvent(button, pressedWidget, referencePx, 0));
            }

            if (pressedWidget != nullptr && !bSuppressClick && !bWasDragging)
            {
                const int32_t clickCount = ResolveClickCount(pressedWidget, button);
                UIPointerEvent clickEvent =
                    MakePointerEvent(button, pressedWidget, referencePx, clickCount);
                m_PointerState.bClickThisFrame = true;
                m_PointerState.Clicked = pressedWidget;
                m_PointerState.ClickButton = button;
                TryDispatchPointerClick(pressedWidget, clickEvent);
            }

            buttonState.Pressed = nullptr;
            buttonState.bDown = false;
            SyncLegacyPointerFlags();
            return;
        }

        if (buttonDown || buttonPressed)
        {
            buttonState.bDown = true;

            if (m_DragSession.Phase == EUIDragPhase::Pending
                && m_DragSession.InitiatingButton == button
                && bHaveReferencePx)
            {
                m_DragSession.CurrentReferencePx = referencePx;
                m_DragSession.PointerDeltaPx = referencePx - m_DragSession.PressReferencePx;
                TryPromotePendingDrag();
            }

            if (m_DragSession.Phase == EUIDragPhase::Dragging
                && m_DragSession.InitiatingButton == button
                && bHaveReferencePx)
            {
                UpdateDragging(m_PointerState.Hovered, referencePx);
            }
        }
        else
        {
            buttonState.bDown = false;
        }

        SyncLegacyPointerFlags();
    }
}
