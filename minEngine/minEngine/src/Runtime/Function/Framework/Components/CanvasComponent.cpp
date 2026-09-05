#include "CanvasComponent.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/UI/UILayoutPass.h"

namespace minEngine
{
    CanvasComponent::CanvasComponent()
    {
        m_bCanEverTick = true;
    }

    void CanvasComponent::Tick(float deltaTime)
    {
        (void)deltaTime;
        if (!IsActive())
        {
            return;
        }

        UILayoutPass::LayoutCanvas(*this);
    }

    void CanvasComponent::SetRenderMode(EUICanvasRenderMode mode)
    {
        m_RenderMode = mode;
    }

    void CanvasComponent::SetScaleMode(EUICanvasScaleMode mode)
    {
        m_ScaleMode = mode;
    }

    void CanvasComponent::SetReferenceResolution(const Vector2& resolution)
    {
        m_ReferenceResolution = resolution;
    }

    UIRect CanvasComponent::GetReferenceRect() const
    {
        UIRect rect;
        rect.TopLeft = Vector2(0.0f, 0.0f);
        rect.Size = Vector2(
            m_ReferenceResolution.x > 0.0f ? m_ReferenceResolution.x : 1.0f,
            m_ReferenceResolution.y > 0.0f ? m_ReferenceResolution.y : 1.0f);
        return rect;
    }

    CanvasComponent* CanvasComponent::FindOwningCanvas(GameObject* gameObject)
    {
        GameObject* current = gameObject;
        while (current != nullptr)
        {
            const std::vector<std::shared_ptr<CanvasComponent>> canvases =
                current->GetComponentsOfType<CanvasComponent>();
            if (!canvases.empty() && canvases[0])
            {
                return canvases[0].get();
            }
            current = current->GetParent();
        }
        return nullptr;
    }
}
