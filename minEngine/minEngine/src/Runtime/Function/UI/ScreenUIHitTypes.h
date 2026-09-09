#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

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
    struct ScreenUIHitQuery
    {
        Vector2 ViewportPoint{ 0.0f, 0.0f };
        float ViewportWidth = 0.0f;
        float ViewportHeight = 0.0f;
        Scene* Scene = nullptr;
    };

    /**
     * Thin pointer interaction state owned by UISystem.
     * Click is an edge flag cleared by the consumer or next Tick.
     */
    struct ScreenUIPointerState
    {
        WidgetComponent* Hovered = nullptr;
        WidgetComponent* Pressed = nullptr;
        bool bPointerDown = false;
        bool bClickThisFrame = false;

        void Reset()
        {
            Hovered = nullptr;
            Pressed = nullptr;
            bPointerDown = false;
            bClickThisFrame = false;
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