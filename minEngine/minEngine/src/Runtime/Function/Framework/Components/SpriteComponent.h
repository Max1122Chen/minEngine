#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/Texture.h"

#include <memory>

namespace minEngine
{
    class Material;
    class Sprite;
    class SpriteSceneProxy;

    ME_CLASS()
    class SpriteComponent : public PrimitiveComponent
    {
        ME_GENERATED_BODY()
    public:
        SpriteComponent();
        virtual ~SpriteComponent() = default;

        void SetSprite(const std::shared_ptr<Sprite>& sprite);
        Sprite* GetSprite() const { return m_Sprite.get(); }
        const std::shared_ptr<Sprite>& GetSpriteShared() const { return m_Sprite; }

        void SetTexture(const std::shared_ptr<Texture2D>& texture);
        Texture2D* GetTexture() const { return m_Texture.get(); }
        const std::shared_ptr<Texture2D>& GetTextureShared() const { return m_Texture; }

        void SetColor(const LinearColor& color);
        LinearColor GetColor() const { return m_Color; }

        void SetSize(const Vector2& size);
        Vector2 GetSize() const { return m_Size; }

        void SetUVRect(const Vector4& uvRect);
        Vector4 GetUVRect() const { return m_UVRect; }

        /** Draw texture: Sprite runtime when set, else legacy m_Texture. */
        std::shared_ptr<Texture2D> ResolveDrawTextureShared() const;
        Texture2D* ResolveDrawTexture() const { return ResolveDrawTextureShared().get(); }

        /** Draw UV (u0,v0,u1,v1): Sprite runtime UV when set, else m_UVRect. */
        Vector4 ResolveDrawUVRect() const;

        static bool IsUVRectDegenerate(const Vector4& uvRect);

        virtual Math::Geometry::AABB GetBoundingBox() const override;
        virtual PrimitiveSceneProxy* CreateSceneProxy() override;
        void SyncSceneProxy(PrimitiveSceneProxy& proxy) override;

    private:
        void EnsureRuntimeMaterial();
        void SyncMaterialParameters();
        void FillSceneProxy(SpriteSceneProxy& proxy);

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetSprite", Getter = "GetSpriteShared"))
        std::shared_ptr<Sprite> m_Sprite{ nullptr };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetTexture", Getter = "GetTextureShared"))
        std::shared_ptr<Texture2D> m_Texture{ nullptr };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetColor", Getter = "GetColor"))
        LinearColor m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };

        ME_PROPERTY()
        Vector2 m_Size{ 1.0f, 1.0f };

        /** Legacy path when m_Sprite is null. (u0, v0, u1, v1) OpenGL-style. */
        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetUVRect", Getter = "GetUVRect"))
        Vector4 m_UVRect{ 0.0f, 0.0f, 1.0f, 1.0f };

        std::shared_ptr<Material> m_RuntimeMaterial{ nullptr };
        bool m_bRuntimeMaterialTranslucent = false;
    };
}

#include "Generated/Reflection/SpriteComponent.gen.h"
