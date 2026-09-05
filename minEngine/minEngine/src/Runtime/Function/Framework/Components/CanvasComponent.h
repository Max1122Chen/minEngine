#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/UI/UITypes.h"

namespace minEngine
{
    /**
     * Screen-space UI root: reference resolution + Letterbox mapping for Path B.
     * Child GameObject tree under this owner is the UI tree.
     */
    ME_CLASS()
    class CanvasComponent : public Component
    {
        ME_GENERATED_BODY(CanvasComponent)
    public:
        CanvasComponent();
        virtual ~CanvasComponent() override = default;

        virtual void Tick(float deltaTime) override;

        EUICanvasRenderMode GetRenderMode() const { return m_RenderMode; }
        void SetRenderMode(EUICanvasRenderMode mode);

        EUICanvasScaleMode GetScaleMode() const { return m_ScaleMode; }
        void SetScaleMode(EUICanvasScaleMode mode);

        Vector2 GetReferenceResolution() const { return m_ReferenceResolution; }
        void SetReferenceResolution(const Vector2& resolution);

        UIRect GetReferenceRect() const;

        /** Walk GO parents to the nearest CanvasComponent, or null. */
        static CanvasComponent* FindOwningCanvas(GameObject* gameObject);

    private:
        ME_PROPERTY()
        EUICanvasRenderMode m_RenderMode{ EUICanvasRenderMode::ScreenSpace };

        ME_PROPERTY()
        EUICanvasScaleMode m_ScaleMode{ EUICanvasScaleMode::Letterbox };

        ME_PROPERTY()
        Vector2 m_ReferenceResolution{ 1920.0f, 1080.0f };
    };
}

#include "Generated/Reflection/CanvasComponent.gen.h"
