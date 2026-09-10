#include "Runtime/Function/UI/ScreenUIHitTester.h"

#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"
#include "Runtime/Function/UI/UITypes.h"

namespace minEngine
{
    ScreenUIHitResult ScreenUIHitTester::HitTestAt(const ScreenUIHitQuery& query) const
    {
        ScreenUIHitResult result;
        result.Reset();

        if (query.Scene == nullptr || query.ViewportWidth <= 0.0f || query.ViewportHeight <= 0.0f)
        {
            return result;
        }

        CanvasHitCandidate best{};
        bool bHaveBest = false;

        for (const std::shared_ptr<GameObject>& gameObject : query.Scene->GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            const std::vector<std::shared_ptr<CanvasComponent>> canvases =
                gameObject->GetComponentsOfType<CanvasComponent>();
            for (const std::shared_ptr<CanvasComponent>& canvasPtr : canvases)
            {
                if (!canvasPtr || !canvasPtr->IsActive())
                {
                    continue;
                }

                if (canvasPtr->GetRenderMode() != EUICanvasRenderMode::ScreenSpace)
                {
                    continue;
                }

                CanvasHitCandidate candidate{};
                if (!TryElectInCanvas(
                        *canvasPtr,
                        query.ViewportPoint,
                        query.ViewportWidth,
                        query.ViewportHeight,
                        candidate))
                {
                    continue;
                }

                if (!bHaveBest || candidate.CanvasSortOrder > best.CanvasSortOrder ||
                    (candidate.CanvasSortOrder == best.CanvasSortOrder &&
                     candidate.WidgetStableOrder > best.WidgetStableOrder))
                {
                    best = candidate;
                    bHaveBest = true;
                }
            }
        }

        if (!bHaveBest || best.Widget == nullptr)
        {
            return result;
        }

        result.bHit = true;
        result.Widget = best.Widget;
        result.Canvas = best.Canvas;
        result.GameObject = best.Widget->GetOwner();
        result.PointInReferenceSpace = best.PointInReferenceSpace;
        return result;
    }

    bool ScreenUIHitTester::TryElectInCanvas(
        CanvasComponent& canvas,
        const Vector2& viewportPoint,
        float viewportWidth,
        float viewportHeight,
        CanvasHitCandidate& outCandidate)
    {
        outCandidate = {};

        GameObject* owner = canvas.GetOwner();
        if (owner == nullptr)
        {
            return false;
        }

        const Vector2 ref = canvas.GetReferenceResolution();
        const ScreenUICoords::LetterboxMapping mapping = ScreenUICoords::MakeLetterboxMapping(
            ref.x,
            ref.y,
            viewportWidth,
            viewportHeight);

        Vector2 refPoint{};
        if (!mapping.TryUnmapPoint(viewportPoint, ref.x, ref.y, refPoint))
        {
            return false;
        }

        WidgetComponent* elect = ElectWidgetInSubtree(owner, refPoint);
        if (elect == nullptr)
        {
            return false;
        }

        outCandidate.Canvas = &canvas;
        outCandidate.Widget = elect;
        outCandidate.PointInReferenceSpace = refPoint;
        outCandidate.CanvasSortOrder = canvas.GetSortOrder();
        outCandidate.WidgetStableOrder = elect->GetStableOrder();
        return true;
    }

    WidgetComponent* ScreenUIHitTester::ElectWidgetInSubtree(
        GameObject* root,
        const Vector2& refPoint)
    {
        WidgetComponent* best = nullptr;
        uint32_t bestOrder = 0;
        CollectHitWidgetsRecursive(root, refPoint, best, bestOrder);
        return best;
    }

    void ScreenUIHitTester::CollectHitWidgetsRecursive(
        GameObject* gameObject,
        const Vector2& refPoint,
        WidgetComponent*& inOutBest,
        uint32_t& inOutBestOrder)
    {
        if (gameObject == nullptr)
        {
            return;
        }

        const std::vector<std::shared_ptr<WidgetComponent>> widgets =
            gameObject->GetComponentsOfType<WidgetComponent>();
        if (!widgets.empty() && widgets[0])
        {
            WidgetComponent* widget = widgets[0].get();
            if (widget->IsActive() && widget->IsHitTestVisible() &&
                widget->GetComputedRect().Contains(refPoint))
            {
                const uint32_t order = widget->GetStableOrder();
                if (inOutBest == nullptr || order > inOutBestOrder)
                {
                    inOutBest = widget;
                    inOutBestOrder = order;
                }
            }
        }

        for (GameObject* child : gameObject->GetChildren())
        {
            CollectHitWidgetsRecursive(child, refPoint, inOutBest, inOutBestOrder);
        }
    }
}
