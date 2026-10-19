#pragma once

#include "Runtime/Function/UI/ScreenUIDragTypes.h"
#include "Runtime/Function/UI/ScreenUIHitTester.h"
#include "Runtime/Function/UI/ScreenUIHitTypes.h"
#include "Runtime/Function/UI/ScreenUIPointerTypes.h"

#include <chrono>

namespace minEngine
{
    class Scene;
    class DragSourceComponent;
    class DropTargetComponent;
    class CanvasComponent;
    class WidgetComponent;

    class UISystem
    {
    public:
        UISystem();
        ~UISystem();

        UISystem(const UISystem&) = delete;
        UISystem& operator=(const UISystem&) = delete;

        void Initialize();
        void Shutdown();

        static bool HasInstance();
        static UISystem& Get();

        void Tick(float deltaTime);

        void SetPointerViewportContext(const ScreenUIPointerViewportContext& context);
        const ScreenUIPointerViewportContext& GetPointerViewportContext() const { return m_ViewportContext; }

        const ScreenUIPointerState& GetPointerState() const { return m_PointerState; }
        const ScreenUIDragSession& GetDragSession() const { return m_DragSession; }
        bool IsDragging() const { return m_DragSession.Phase == EUIDragPhase::Dragging; }

        float GetDefaultDragThresholdPx() const { return m_DefaultDragThresholdPx; }
        void SetDefaultDragThresholdPx(float thresholdPx);

        Vector2 GetDragPointerReferencePx() const { return m_DragSession.CurrentReferencePx; }

        ScreenUIHitResult HitTestAt(const ScreenUIHitQuery& query) const;
        ScreenUIHitResult HitTestAtWindowMouse(Scene* scene) const;

        void SetPointerRoutingEnabled(bool enabled);
        bool IsPointerRoutingEnabled() const { return m_bPointerRoutingEnabled; }

        /**
         * True when ScreenUI should consume the pointer for world/editor pick.
         * Call from pick / gameplay consumers (S2). Requires routing enabled.
         */
        bool ShouldBlockWorldPointer() const;

        /**
         * Re-sample pointer against the current viewport context.
         * Editor injects ImageMin/Size then calls this so Hover matches the drawn frame.
         */
        void PollPointer();

        void OnBeginPIE(Scene* pieScene);
        void OnEndPIE(Scene* pieScene);

        /** Cancel active/pending drag if source matches (e.g. component deactivate). */
        void CancelDragIfSource(DragSourceComponent* source);

    private:
        friend class Engine;
        friend class ScreenUIHitTestScope;
        friend class ScreenUIButtonTestScope;
        friend class ScreenUIDragTestScope;
        friend class ScreenUIPointerTestScope;

        static void SetInstance(UISystem* instance);

        void UpdatePointerFromInput(Scene* scene);
        void UpdatePointerFrame(
            Scene* scene,
            const ScreenUIHitResult& hit,
            const Vector2& referencePx,
            bool bHaveReferencePx,
            EUIPointerButton button,
            bool buttonPressed,
            bool buttonDown,
            bool buttonReleased);

        /** Legacy Left-only helper for existing drag tests. */
        void UpdatePointerFrame(
            Scene* scene,
            const ScreenUIHitResult& hit,
            const Vector2& referencePx,
            bool bHaveReferencePx,
            bool buttonPressed,
            bool buttonDown,
            bool buttonReleased);

        Scene* ResolvePointerScene() const;
        void TryDispatchButtonClick(WidgetComponent* pressedWidget);
        void TryDispatchPointerClick(WidgetComponent* widget, const UIPointerEvent& event);

        void SampleModifiers();
        UIPointerEvent MakePointerEvent(
            EUIPointerButton button,
            WidgetComponent* widget,
            const Vector2& referencePx,
            int32_t clickCount) const;
        void UpdateHover(WidgetComponent* newHovered, const Vector2& referencePx);
        void ProcessButtonEdges(
            EUIPointerButton button,
            bool buttonPressed,
            bool buttonDown,
            bool buttonReleased,
            WidgetComponent* hitWidget,
            const Vector2& referencePx,
            bool bHaveReferencePx);
        void SyncLegacyPointerFlags();
        int32_t ResolveClickCount(WidgetComponent* widget, EUIPointerButton button);

        bool TryUnmapReferencePx(
            CanvasComponent& canvas,
            const Vector2& viewportPoint,
            float viewportWidth,
            float viewportHeight,
            Vector2& outReferencePx) const;

        DragSourceComponent* FindDragSource(WidgetComponent* widget) const;
        DropTargetComponent* FindDropTarget(WidgetComponent* widget) const;

        void BeginPendingDrag(WidgetComponent* widget, const Vector2& referencePx, EUIPointerButton button);
        void TryPromotePendingDrag();
        void UpdateDragging(WidgetComponent* hoveredWidget, const Vector2& referencePx);
        void FinishDragging(WidgetComponent* hoveredWidget, bool bReleased);
        void ClearDragSession();
        void CancelDragSession();

        static UISystem* s_Instance;

        bool m_Initialized = false;
        bool m_bPointerRoutingEnabled = false;
        float m_DefaultDragThresholdPx = 6.0f;
        float m_DoubleClickSeconds = 0.3f;

        ScreenUIHitTester m_HitTester;
        ScreenUIPointerState m_PointerState;
        ScreenUIPointerViewportContext m_ViewportContext;
        ScreenUIDragSession m_DragSession;
        Scene* m_PIEScene = nullptr;

        WidgetComponent* m_LastClickWidget = nullptr;
        EUIPointerButton m_LastClickButton = EUIPointerButton::None;
        std::chrono::steady_clock::time_point m_LastClickTime{};
    };
}
