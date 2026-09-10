#include "UILayoutPass.h"

#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <cmath>

namespace minEngine
{
    void UILayoutPass::LayoutCanvas(CanvasComponent& canvas)
    {
        GameObject* owner = canvas.GetOwner();
        if (owner == nullptr || !canvas.IsActive())
        {
            return;
        }

        const UIRect refRect = canvas.GetReferenceRect();
        LayoutGameObject(owner, refRect);
    }

    void UILayoutPass::LayoutGameObject(GameObject* gameObject, const UIRect& parentRect)
    {
        if (gameObject == nullptr)
        {
            return;
        }

        UIRect childParentRect = parentRect;
        const std::vector<std::shared_ptr<WidgetComponent>> widgets =
            gameObject->GetComponentsOfType<WidgetComponent>();
        if (!widgets.empty() && widgets[0] && widgets[0]->IsActive())
        {
            WidgetComponent& widget = *widgets[0];
            const UIRect computed = ComputeChildRect(parentRect, widget);
            widget.SetComputedRect(computed);
            childParentRect = computed;
        }

        for (GameObject* child : gameObject->GetChildren())
        {
            LayoutGameObject(child, childParentRect);
        }
    }

    bool UILayoutPass::IsPointAnchor(const Vector2& anchorMin, const Vector2& anchorMax)
    {
        constexpr float kEpsilon = 1.0e-4f;
        return std::fabs(anchorMin.x - anchorMax.x) <= kEpsilon
            && std::fabs(anchorMin.y - anchorMax.y) <= kEpsilon;
    }

    UIRect UILayoutPass::ComputeChildRect(const UIRect& parentRect, const WidgetComponent& widget)
    {
        const Vector2 anchorMin = widget.GetAnchorMin();
        const Vector2 anchorMax = widget.GetAnchorMax();
        const Vector4 margin = widget.GetMargin();
        const Vector2 size = widget.GetSize();

        const float anchorLeft = parentRect.Left() + anchorMin.x * parentRect.Width();
        const float anchorTop = parentRect.Top() + anchorMin.y * parentRect.Height();
        const float anchorRight = parentRect.Left() + anchorMax.x * parentRect.Width();
        const float anchorBottom = parentRect.Top() + anchorMax.y * parentRect.Height();

        if (IsPointAnchor(anchorMin, anchorMax))
        {
            const float left = anchorLeft + margin.x;
            const float top = anchorTop + margin.y;
            const float width = size.x > 0.0f ? size.x : 0.0f;
            const float height = size.y > 0.0f ? size.y : 0.0f;
            return UIRect::FromLTRB(left, top, left + width, top + height);
        }

        float left = anchorLeft + margin.x;
        float top = anchorTop + margin.y;
        float right = anchorRight - margin.z;
        float bottom = anchorBottom - margin.w;

        if (right < left)
        {
            right = left;
        }
        if (bottom < top)
        {
            bottom = top;
        }

        return UIRect::FromLTRB(left, top, right, bottom);
    }
}
