#include "ScreenUIMaterialFactory.h"

#include "Runtime/Function/Render/Sprite/SpriteMaterialFactory.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    namespace
    {
        /** Process-lifetime white texture; owns a GPU texture (cleared by RenderSystem::Shutdown). */
        std::shared_ptr<Texture2D>& CachedWhiteTexture()
        {
            static std::shared_ptr<Texture2D> s_WhiteTexture;
            return s_WhiteTexture;
        }
    }

    std::shared_ptr<Texture2D> ScreenUIMaterialFactory::GetOrCreateWhiteTexture(RHI& rhi)
    {
        std::shared_ptr<Texture2D>& whiteTexture = CachedWhiteTexture();
        if (!whiteTexture || whiteTexture->GetRHITexture() == nullptr)
        {
            whiteTexture = Texture2D::CreateSolidRGBA(rhi, 255, 255, 255, 255);
        }
        return whiteTexture;
    }

    void ScreenUIMaterialFactory::ClearCachedResources()
    {
        // Refcount-safe: widget Materials keep their own shared_ptr to the texture they bound.
        CachedWhiteTexture().reset();
    }

    bool ScreenUIMaterialFactory::HasCachedWhiteTexture()
    {
        return CachedWhiteTexture() != nullptr;
    }

    std::shared_ptr<Material> ScreenUIMaterialFactory::CreateInstance(RHI& rhi)
    {
        return SpriteMaterialFactory::CreateInstance(rhi, true);
    }

    void ScreenUIMaterialFactory::ApplyColorAndTexture(
        Material& material,
        const LinearColor& color,
        const std::shared_ptr<Texture2D>& texture,
        RHI& rhi)
    {
        ApplyColorTextureAndUVRect(
            material,
            color,
            texture,
            Vector4(0.0f, 0.0f, 1.0f, 1.0f),
            rhi);
    }

    void ScreenUIMaterialFactory::ApplyColorTextureAndUVRect(
        Material& material,
        const LinearColor& color,
        const std::shared_ptr<Texture2D>& texture,
        const Vector4& uvRect,
        RHI& rhi)
    {
        const std::shared_ptr<Texture2D> resolved =
            texture ? texture : GetOrCreateWhiteTexture(rhi);
        SpriteMaterialFactory::ApplyColorTextureAndUVRect(material, color, resolved, uvRect);
    }
}
