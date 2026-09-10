#pragma once

#include "Core.h"
#include "Runtime/Function/UI/UITypes.h"

namespace minEngine
{
    class CanvasComponent;
    class GameObject;
    class WidgetComponent;

    /** Depth-first Canvas subtree layout into Widget ComputedRect (reference pixels). */
    class UILayoutPass
    {
    public:
        static void LayoutCanvas(CanvasComponent& canvas);

        /** Pure layout math used by LayoutCanvas and tests. */
        static UIRect ComputeChildRect(const UIRect& parentRect, const WidgetComponent& widget);

    private:
        static void LayoutGameObject(GameObject* gameObject, const UIRect& parentRect);
        static bool IsPointAnchor(const Vector2& anchorMin, const Vector2& anchorMax);
    };
}
