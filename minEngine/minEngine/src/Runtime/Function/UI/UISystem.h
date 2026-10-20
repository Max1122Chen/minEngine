#pragma once

#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"
#include "Runtime/Function/Render/WindowSystem.h"
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

        /**
         * ScreenUI viewport size in WINDOW pixels (WindowSystem::GetWidth()/GetHeight() space,
         * top-left origin, +Y down) - the SAME space as the mouse position, ScreenUI hit tests and
         * the letterbox math. It is NOT framebuffer pixels: on Windows with display scaling the GL
         * framebuffer is larger than the window, and the engine has no framebuffer-size source.
         *
         * Precedence: the injected viewport context when valid (a host that renders ScreenUI into a
         * sub-rect, e.g. an editor Play viewport), else the platform window, else (0,0).
         */
        Vector2 GetViewportSizeWindowPx() const
        {
            if (m_ViewportContext.bValid && m_ViewportContext.ImageSize.x > 0.0f
                && m_ViewportContext.ImageSize.y > 0.0f)
            {
                return m_ViewportContext.ImageSize;
            }

            if (!WindowSystem::HasInstance())
            {
                return Vector2(0.0f, 0.0f);
            }

            return Vector2(
                static_cast<float>(WindowSystem::Get().GetWidth()),
                static_cast<float>(WindowSystem::Get().GetHeight()));
        }

        /**
         * Letterbox mapping Canvas reference px -> viewport window px, built with
         * ScreenUICoords::MakeLetterboxMapping (the one conversion ScreenUI already uses).
         * Scale == 0 is the "no valid mapping" signal (null canvas, non-positive reference
         * resolution or viewport) - it is never a sanitised 1:1 mapping.
         */
        ScreenUICoords::LetterboxMapping GetReferencePxMapping(const CanvasComponent* canvas) const
        {
            ScreenUICoords::LetterboxMapping mapping;
            if (canvas == nullptr)
            {
                mapping.Scale = 0.0f;
                return mapping;
            }

            const Vector2 reference = canvas->GetReferenceResolution();
            const Vector2 viewport = GetViewportSizeWindowPx();
            if (reference.x <= 0.0f || reference.y <= 0.0f || viewport.x <= 0.0f || viewport.y <= 0.0f)
            {
                mapping.Scale = 0.0f;
                return mapping;
            }

            return ScreenUICoords::MakeLetterboxMapping(reference.x, reference.y, viewport.x, viewport.y);
        }

        /**
         * Inverse of GetReferencePxMapping: viewport window px -> Canvas reference px.
         * False when the mapping is invalid or the point falls in the letterbox bars
         * (outReferencePx is left untouched, so no fake coordinate escapes).
         */
        bool TryUnmapViewportPointToReferencePx(
            const CanvasComponent* canvas,
            const Vector2& viewportPointWindowPx,
            Vector2& outReferencePx) const
        {
            const ScreenUICoords::LetterboxMapping mapping = GetReferencePxMapping(canvas);
            if (mapping.Scale <= 0.0f)
            {
                return false;
            }

            const Vector2 reference = canvas->GetReferenceResolution();
            return mapping.TryUnmapPoint(viewportPointWindowPx, reference.x, reference.y, outReferencePx);
        }

        /**
         * Latest pointer sample in Canvas reference px. Same source as UIPointerEvent::ReferencePx:
         * both are filled from one derivation inside UISystem, so a host can read the pointer here
         * and receive exactly the coordinates its UI event handlers see.
         * bValid == false until the engine has a viewport context and a pointer frame.
         */
        const ScreenUIPointerViewportSample& GetPointerPosition() const { return m_PointerSample; }

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
        friend class ScreenUIGameViewportTestScope;

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

        /**
         * The single reference-px derivation for both the dispatched pointer events and
         * GetPointerPosition(): ScreenUI hit tester unmap, else the active drag canvas unmap.
         */
        bool DerivePointerReferencePx(
            Scene* scene,
            ScreenUIHitResult& outHit,
            Vector2& outReferencePx) const;
        void RecordPointerSample(const Vector2& referencePx, bool bHaveReferencePx);
        /** Sample without dispatching events (no pointer routing, engine-owned viewport). */
        void UpdatePointerSample(Scene* scene);
        /** Engine feeds the game viewport (whole window) unless a host injected its own surface. */
        void UpdateEngineDefaultViewportContext();
        void ResetPointerViewportContext();

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

        // Appended (never inserted): keeps every pre-existing member offset stable for consumers
        // that were compiled against the previous header.
        ScreenUIPointerViewportSample m_PointerSample;
        /** True while UISystem itself owns m_ViewportContext (engine default = whole window). */
        bool m_bViewportContextFromEngine = true;
    };
}
