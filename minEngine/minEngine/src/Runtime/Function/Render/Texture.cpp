#include "Texture.h"
#include "RenderSystem.h"
#include "RHI/RHI.h"
#include "RHI/RHITexture.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    TextureCube::TextureCube(const std::vector<std::string> &facePaths, uint32_t unit, TextureWrapping wrapping, TextureFiltering filtering)
        : m_Wrapping(wrapping), m_Filtering(filtering)
    {
        RenderSystem& renderSystem = RenderSystem::GetRenderSystem();
        RHI* rhi = renderSystem.GetRHI();

        // Load textures for each face
        std::vector<unsigned char*> faceData(6);

        int width, height, channels;
        for (int i = 0; i < 6; ++i)
        {
            faceData[i] = AssetManager::GetAssetManager().LoadImage(facePaths[i], width, height, channels);
            if(width != height)
            {
                ME_CORE_ERROR("Cubemap face textures must be square. Invalid texture: {}", facePaths[0]);
                return;
            }
        }
        m_Size = static_cast<uint32_t>(width); // Assuming all faces are the same size
        m_Channels = static_cast<uint32_t>(channels);

        // Create RHI cube texture. Directly call rhi, maybe later we should use RHICommand.
        m_RHITexture = rhi->CreateRHITextureCube(faceData, RHITextureDesc{
            .Width = m_Size,
            .Height = m_Size,
            .Format = (channels == 4) ? TextureFormat::RGBA8 : TextureFormat::RGB8,
            .Usage = TextureUsage::TextureBinding
        }, static_cast<int>(unit));

        for(int i = 0; i < 6; ++i)
        {
            AssetManager::GetAssetManager().FreeImage(faceData[i]);
        }
    }

}