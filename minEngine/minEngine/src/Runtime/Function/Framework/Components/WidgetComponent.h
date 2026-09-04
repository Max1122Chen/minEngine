#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    class Material;
    class WidgetSceneProxy;

    ME_CLASS()
    class WidgetComponent : public SceneComponent
    {
        ME_GENERATED_BODY(WidgetComponent)
    public:
        WidgetComponent();
        virtual ~WidgetComponent() override;

        void SetTexture(const std::shared_ptr<Texture2D>& texture);
        Texture2D* GetTexture() const { return m_Texture.get(); }

        void SetColor(const Vector4& color);
        Vector4 GetColor() const { return m_Color; }

        void SetSize(const Vector2& size);
        Vector2 GetSize() const { return m_Size; }

        void SetStableOrder(uint32_t order);
        uint32_t GetStableOrder() const { return m_StableOrder; }

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
        void SyncMaterialParameters();
        void FillSceneProxy(WidgetSceneProxy& proxy);

        ME_PROPERTY()
        std::shared_ptr<Texture2D> m_Texture{ nullptr };

        ME_PROPERTY()
        Vector4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };

        /** Pixel size (width, height). Location.xy = top-left in viewport pixels. */
        ME_PROPERTY()
        Vector2 m_Size{ 100.0f, 100.0f };

        ME_PROPERTY()
        uint32_t m_StableOrder = 0;

        std::shared_ptr<Material> m_RuntimeMaterial{ nullptr };
        WidgetSceneProxy* m_WidgetSceneProxy = nullptr;
    };
}

#include "Generated/Reflection/WidgetComponent.gen.h"
