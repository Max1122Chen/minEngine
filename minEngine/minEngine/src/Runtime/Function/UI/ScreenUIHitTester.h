#pragma once

#include "Runtime/Function/UI/ScreenUIHitTypes.h"

namespace minEngine
{
    class CanvasComponent;
    class WidgetComponent;
    class GameObject;

    /**
     * Stateless ScreenUI hit query (multi-Canvas two-phase election).
     * Not an engine subsystem; called by UISystem / tests.
     */
    class ScreenUIHitTester
    {
    public:
        ScreenUIHitTester() = default;

        /** Front-most hit Widget under the query, or empty result. */
        ScreenUIHitResult HitTestAt(const ScreenUIHitQuery& query) const;

    private:
        struct CanvasHitCandidate
        {
            CanvasComponent* Canvas = nullptr;
            WidgetComponent* Widget = nullptr;
            Vector2 PointInReferenceSpace{ 0.0f, 0.0f };
            int32_t CanvasSortOrder = 0;
            uint32_t WidgetStableOrder = 0;
        };

        static bool TryElectInCanvas(
            CanvasComponent& canvas,
            const Vector2& viewportPoint,
            float viewportWidth,
            float viewportHeight,
            CanvasHitCandidate& outCandidate);

        static WidgetComponent* ElectWidgetInSubtree(
            GameObject* root,
            const Vector2& refPoint);

        static void CollectHitWidgetsRecursive(
            GameObject* gameObject,
            const Vector2& refPoint,
            WidgetComponent*& inOutBest,
            uint32_t& inOutBestOrder);
    };
}
