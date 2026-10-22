#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/UI/ScreenUIPointerTypes.h"

namespace minEngine
{
    class WidgetComponent;
    class CanvasComponent;
    class GameObject;
    class Scene;

    /** Result of a ScreenUI pointer hit query (reference-space aware). */
    struct ScreenUIHitResult
    {
        WidgetComponent* Widget = nullptr;
        GameObject* GameObject = nullptr;
        CanvasComponent* Canvas = nullptr;
        Vector2 PointInReferenceSpace{ 0.0f, 0.0f };
        bool bHit = false;

        void Reset()
        {
            Widget = nullptr;
            GameObject = nullptr;
            Canvas = nullptr;
            PointInReferenceSpace = Vector2(0.0f, 0.0f);
            bHit = false;
        }
    };

    /**
     * Inputs for HitTestAt. Viewport pixels are top-left origin (+Y down),
     * in the same space as ScreenUI Letterbox Map/Unmap (image-local, not window).
     */
    struct MINENGINE_API ScreenUIHitQuery
    {
        Vector2 ViewportPoint{ 0.0f, 0.0f };
        float ViewportWidth = 0.0f;
        float ViewportHeight = 0.0f;
        Scene* Scene = nullptr;
    };

    struct ScreenUIPerButtonPointerState
    {
        WidgetComponent* Pressed = nullptr;
        bool bDown = false;

        void Reset()
        {
            Pressed = nullptr;
            bDown = false;
        }
    };

    /**
     * Pointer interaction state owned by UISystem.
     * Click is an edge flag cleared by the consumer or next Tick.
     */
    struct ScreenUIPointerState
    {
        WidgetComponent* Hovered = nullptr;
        /** Convenience: Left.Pressed (Button tint / legacy). */
        WidgetComponent* Pressed = nullptr;
        /** Widget that owned the Click edge this frame (valid while bClickThisFrame). */
        WidgetComponent* Clicked = nullptr;
        EUIPointerButton ClickButton = EUIPointerButton::None;
        bool bPointerDown = false;
        bool bClickThisFrame = false;
        /** True when any ButtonComponent::OnClicked was broadcast this frame. */
        bool bButtonOnClickedThisFrame = false;
        UIKeyModifiers Modifiers{};

        ScreenUIPerButtonPointerState Left{};
        ScreenUIPerButtonPointerState Right{};
        ScreenUIPerButtonPointerState Middle{};

        void Reset()
        {
            Hovered = nullptr;
            Pressed = nullptr;
            Clicked = nullptr;
            ClickButton = EUIPointerButton::None;
            bPointerDown = false;
            bClickThisFrame = false;
            bButtonOnClickedThisFrame = false;
            Modifiers = UIKeyModifiers{};
            Left.Reset();
            Right.Reset();
            Middle.Reset();
        }

        ScreenUIPerButtonPointerState& GetButtonState(EUIPointerButton button)
        {
            switch (button)
            {
                case EUIPointerButton::Right: return Right;
                case EUIPointerButton::Middle: return Middle;
                case EUIPointerButton::Left:
                default: return Left;
            }
        }

        const ScreenUIPerButtonPointerState& GetButtonState(EUIPointerButton button) const
        {
            switch (button)
            {
                case EUIPointerButton::Right: return Right;
                case EUIPointerButton::Middle: return Middle;
                case EUIPointerButton::Left:
                default: return Left;
            }
        }
    };

    /**
     * Latest ScreenUI pointer sample, in Canvas reference pixels (the space ScreenUI widgets are
     * laid out in: top-left origin, +Y down, no letterbox applied).
     *
     * ReferencePx is produced by the SAME derivation that fills UIPointerEvent::ReferencePx
     * (hit-tester unmap, else the active drag canvas unmap) - there is no second conversion and
     * no second pixel space.
     */
    struct ScreenUIPointerViewportSample
    {
        /** Pointer in Canvas reference pixels. Meaningless unless bValid. */
        Vector2 ReferencePx{ 0.0f, 0.0f };

        /** ReferencePx minus the previous valid ReferencePx (reference pixels, not viewport pixels). */
        Vector2 ReferenceDeltaPx{ 0.0f, 0.0f };

        /**
         * False when nothing could be sampled: no viewport context, no platform window / pointer
         * frame, or the pointer lies outside the letterboxed content rect (black bars).
         * A false sample never carries a "made up" position - both vectors stay zero.
         */
        bool bValid = false;

        void Reset()
        {
            ReferencePx = Vector2(0.0f, 0.0f);
            ReferenceDeltaPx = Vector2(0.0f, 0.0f);
            bValid = false;
        }
    };

    /**
     * Viewport surface used when converting window mouse to image-local pixels.
     * Editor / Play inject this; HitTester itself only sees viewport-local points.
     */
    struct ScreenUIPointerViewportContext
    {
        Vector2 ImageMin{ 0.0f, 0.0f };
        Vector2 ImageSize{ 0.0f, 0.0f };
        bool bValid = false;

        void Reset()
        {
            ImageMin = Vector2(0.0f, 0.0f);
            ImageSize = Vector2(0.0f, 0.0f);
            bValid = false;
        }
    };
}
