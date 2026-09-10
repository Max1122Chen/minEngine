#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    class Material;
    class SpriteSceneProxy;

    ME_CLASS()
    class SpriteComponent : public PrimitiveComponent
    {
        ME_GENERATED_BODY()
    public:
        SpriteComponent();
        virtual ~SpriteComponent() = default;

        void SetTexture(const std::shared_ptr<Texture2D>& texture);
        Texture2D* GetTexture() const { return m_Texture.get(); }
        const std::shared_ptr<Texture2D>& GetTextureShared() const { return m_Texture; }

        void SetColor(const LinearColor& color);
        LinearColor GetColor() const { return m_Color; }

        void SetSize(const Vector2& size);
        Vector2 GetSize() const { return m_Size; }

        void SetUVRect(const Vector4& uvRect);
        Vector4 GetUVRect() const { return m_UVRect; }

        virtual Math::Geometry::AABB GetBoundingBox() const override;
        virtual PrimitiveSceneProxy* CreateSceneProxy() override;

        /** Refresh an existing SpriteSceneProxy (RenderScene dirty update). */
        void UpdateSceneProxy(SpriteSceneProxy& proxy);

    private:
        void EnsureRuntimeMaterial();
        void SyncMaterialParameters();
        void FillSceneProxy(SpriteSceneProxy& proxy);

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetTexture", Getter = "GetTextureShared"))
        std::shared_ptr<Texture2D> m_Texture{ nullptr };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetColor", Getter = "GetColor"))
        LinearColor m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };

        ME_PROPERTY()
        Vector2 m_Size{ 1.0f, 1.0f };

        /** (u0, v0, u1, v1). Non-default remap is stored; GPU remap may be deferred. */
        ME_PROPERTY()
        Vector4 m_UVRect{ 0.0f, 0.0f, 1.0f, 1.0f };

        std::shared_ptr<Material> m_RuntimeMaterial{ nullptr };
        bool m_bRuntimeMaterialTranslucent = false;
    };
}

#include "Generated/Reflection/SpriteComponent.gen.h"
