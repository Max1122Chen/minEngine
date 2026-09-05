#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/UI/UITypes.h"

namespace minEngine
{
    class Material;
    class WidgetSceneProxy;
    class ImageComponent;

    /**
     * ScreenUI layout node: Anchor/Margin/Size → ComputedRect (Canvas reference pixels).
     * Visual style comes from a sibling ImageComponent.
     */
    ME_CLASS()
    class WidgetComponent : public SceneComponent
    {
        ME_GENERATED_BODY(WidgetComponent)
    public:
        WidgetComponent();
        virtual ~WidgetComponent() override;

        void SetSize(const Vector2& size);
        Vector2 GetSize() const { return m_Size; }

        void SetAnchorMin(const Vector2& anchorMin);
        Vector2 GetAnchorMin() const { return m_AnchorMin; }

        void SetAnchorMax(const Vector2& anchorMax);
        Vector2 GetAnchorMax() const { return m_AnchorMax; }

        /** Margin as (Left, Top, Right, Bottom) in reference pixels. */
        void SetMargin(const Vector4& margin);
        Vector4 GetMargin() const { return m_Margin; }

        void SetAnchorPreset(EUIAnchorPreset preset);
        EUIAnchorPreset GetAnchorPreset() const { return m_AnchorPreset; }

        /** Writes AnchorMin/Max from preset; clears Margin; keeps Size. */
        void ApplyAnchorPreset(EUIAnchorPreset preset);

        void SetStableOrder(uint32_t order);
        uint32_t GetStableOrder() const { return m_StableOrder; }

        const UIRect& GetComputedRect() const { return m_ComputedRect; }
        void SetComputedRect(const UIRect& rect);

        ImageComponent* FindSiblingImage() const;

        virtual void DoEndOfFrameUpdate() override;

        WidgetSceneProxy* CreateSceneProxy();
        WidgetSceneProxy* GetSceneProxy() const { return m_WidgetSceneProxy; }
        void DetachSceneProxy() { m_WidgetSceneProxy = nullptr; }

        void UpdateSceneProxy(WidgetSceneProxy& proxy);

    protected:
        void ApplyActivationToSystems() override;
        void RemoveActivationFromSystems() override;

    private:
        void EnsureRuntimeMaterial();
        void SyncMaterialParameters(ImageComponent* image);
        void FillSceneProxy(WidgetSceneProxy& proxy);

        ME_PROPERTY()
        EUIAnchorPreset m_AnchorPreset{ EUIAnchorPreset::TopLeft };

        ME_PROPERTY()
        Vector2 m_AnchorMin{ 0.0f, 0.0f };

        ME_PROPERTY()
        Vector2 m_AnchorMax{ 0.0f, 0.0f };

        ME_PROPERTY()
        Vector4 m_Margin{ 0.0f, 0.0f, 0.0f, 0.0f };

        /** Fixed size when anchors form a point; ignored when stretching. */
        ME_PROPERTY()
        Vector2 m_Size{ 100.0f, 100.0f };

        ME_PROPERTY()
        uint32_t m_StableOrder = 0;

        UIRect m_ComputedRect{};

        std::shared_ptr<Material> m_RuntimeMaterial{ nullptr };
        WidgetSceneProxy* m_WidgetSceneProxy = nullptr;
    };
}

#include "Generated/Reflection/WidgetComponent.gen.h"
