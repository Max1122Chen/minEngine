#include "Texture.h"
#include "RenderSystem.h"
#include "RHI/RHI.h"
#include "RHI/RHITexture.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    Texture2D::Texture2D(const std::string& path, uint32_t unit,
                         TextureWrapping wrapping,
                         TextureFiltering filtering)
        : m_Wrapping(wrapping), m_Filtering(filtering)
    {
        RenderSystem& renderSystem = RenderSystem::GetRenderSystem();
        RHI* rhi = renderSystem.GetRHI();

        // Load texture
        AssetManager& assetManager = AssetManager::GetAssetManager();
        int width, height, channels;
        unsigned char* data = assetManager.LoadImage(path, width, height, channels);
        m_Width = static_cast<uint32_t>(width);
        m_Height = static_cast<uint32_t>(height);
        m_Channels = static_cast<uint32_t>(channels);

        if(data)
        {
            // Create RHI texture. Directly call rhi, maybe later we should use RHICommand.
            m_RHITexture = rhi->CreateRHITexture2D(data, RHITextureDesc{
                .Width = m_Width,
                .Height = m_Height,
                .Format = (channels == 4) ? TextureFormat::RGBA8 : TextureFormat::RGB8,
                .Usage = TextureUsage::TextureBinding
            }, static_cast<int>(unit));
            assetManager.FreeImage(data);
        }
    }

}