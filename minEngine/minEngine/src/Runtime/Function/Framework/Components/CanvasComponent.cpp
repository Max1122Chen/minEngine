#include "CanvasComponent.h"

#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/UI/UILayoutPass.h"

namespace minEngine
{
    namespace
    {
        /**
         * Dirty the widgets on this node and every node below it.
         * A widget's draw bit is decided when its proxy refills, and the canvas it belongs to is
         * part of that decision; without this wake-up switching a canvas off leaves the last
         * picture on screen until something unrelated happens to rebuild the proxies.
         */
        void MarkSubtreeWidgetsDirty(GameObject& node)
        {
            for (const std::shared_ptr<Component>& component : node.GetAllComponents())
            {
                if (!component || component->GetClass() == nullptr ||
                    !component->IsA(WidgetComponent::StaticClass()))
                {
                    continue;
                }

                static_cast<WidgetComponent*>(component.get())->MarkRenderStateDirty();
            }

            for (GameObject* child : node.GetChildren())
            {
                if (child != nullptr)
                {
                    MarkSubtreeWidgetsDirty(*child);
                }
            }
        }
    }

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

    void CanvasComponent::OnActivate()
    {
        if (GameObject* owner = GetOwner())
        {
            MarkSubtreeWidgetsDirty(*owner);
        }
    }

    void CanvasComponent::OnDeactivate()
    {
        if (GameObject* owner = GetOwner())
        {
            MarkSubtreeWidgetsDirty(*owner);
        }
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

    void CanvasComponent::SetSortOrder(int32_t sortOrder)
    {
        m_SortOrder = sortOrder;
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
