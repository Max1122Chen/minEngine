#include "ScreenUIMaterialFactory.h"

#include "Runtime/Function/Render/Sprite/SpriteMaterialFactory.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    std::shared_ptr<Texture2D> ScreenUIMaterialFactory::GetOrCreateWhiteTexture(RHI& rhi)
    {
        static std::shared_ptr<Texture2D> s_WhiteTexture;
        if (!s_WhiteTexture || s_WhiteTexture->GetRHITexture() == nullptr)
        {
            s_WhiteTexture = Texture2D::CreateSolidRGBA(rhi, 255, 255, 255, 255);
        }
        return s_WhiteTexture;
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
        const std::shared_ptr<Texture2D> resolved =
            texture ? texture : GetOrCreateWhiteTexture(rhi);
        SpriteMaterialFactory::ApplyColorAndTexture(material, color, resolved);
    }
}
