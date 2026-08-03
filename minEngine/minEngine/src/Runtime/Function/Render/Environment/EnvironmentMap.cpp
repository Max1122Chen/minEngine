#include "EnvironmentMap.h"

#include "EnvMapCapture.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Render/TextureCubeLoader.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"

#include <array>
#include <filesystem>

namespace minEngine
{
    namespace
    {
        std::shared_ptr<TextureCube> CreateValidationEnvironmentCube(RHI& rhi)
        {
            const std::array<uint8_t, 4> faceColors[6] = {
                std::array<uint8_t, 4>{64, 128, 255, 255},
                std::array<uint8_t, 4>{32, 64, 128, 255},
                std::array<uint8_t, 4>{128, 192, 255, 255},
                std::array<uint8_t, 4>{16, 32, 64, 255},
                std::array<uint8_t, 4>{96, 160, 255, 255},
                std::array<uint8_t, 4>{48, 96, 192, 255},
            };
            std::string error;
            std::shared_ptr<TextureCube> cube =
                TextureCubeLoader::CreateSolidColorCube(rhi, 32, faceColors, &error);
            if (!cube)
            {
                ME_CORE_ERROR("EnvironmentMap: validation cubemap failed: {}", error);
            }
            return cube;
        }

        std::filesystem::path ResolveProjectRelativeAbsolute(const std::string& projectRelativePath)
        {
            if (projectRelativePath.empty())
            {
                return {};
            }

            const std::filesystem::path& contentRoot = PathRegistry::Get().GetProjectContentRoot();
            if (contentRoot.empty())
            {
                return {};
            }

            return std::filesystem::weakly_canonical(contentRoot / projectRelativePath);
        }
    }

    bool EnvironmentMap::TryBakeFromSourceHdr(RHI& rhi)
    {
        const std::filesystem::path hdrAbsolute = ResolveProjectRelativeAbsolute(m_SourceHdrPath);
        if (hdrAbsolute.empty() || !std::filesystem::exists(hdrAbsolute))
        {
            ME_CORE_WARN(
                "EnvironmentMap: SourceHdrPath '{}' not found under Project Content.",
                m_SourceHdrPath);
            return false;
        }

        const std::filesystem::path& engineDefaultAssetsRoot =
            PathRegistry::Get().GetEngineDefaultAssetsRoot();
        if (engineDefaultAssetsRoot.empty())
        {
            ME_CORE_ERROR("EnvironmentMap: EngineDefaultAssetsRoot empty; cannot load EnvMap shaders.");
            return false;
        }

        ImagePixels hdrPixels;
        std::string loadError;
        if (!ImageLoader::LoadHdr(hdrAbsolute.string(), hdrPixels, false, &loadError))
        {
            ME_CORE_WARN(
                "EnvironmentMap: failed to load HDR '{}' ({}).",
                hdrAbsolute.string(),
                loadError);
            return false;
        }

        std::shared_ptr<Texture2D> equirect =
            Texture2DLoader::CreateFromHdrPixels(rhi, hdrPixels, hdrAbsolute.filename().string());
        ImageLoader::Free(hdrPixels);
        if (!equirect || !equirect->GetRHITexture())
        {
            ME_CORE_WARN("EnvironmentMap: failed to upload HDR '{}' to GPU.", hdrAbsolute.string());
            return false;
        }

        std::string captureError;
        m_Environment = EnvMapCapture::EquirectToCubemap(
            rhi,
            *equirect->GetRHITexture(),
            engineDefaultAssetsRoot,
            EnvMapCapture::kDefaultCubeFaceSize,
            &captureError);
        if (!m_Environment)
        {
            ME_CORE_WARN(
                "EnvironmentMap: EquirectToCubemap failed for '{}' ({}).",
                hdrAbsolute.string(),
                captureError);
            return false;
        }

        std::string irradianceError;
        m_Irradiance = EnvMapCapture::ConvolveIrradiance(
            rhi,
            *m_Environment->GetRHITexture(),
            engineDefaultAssetsRoot,
            EnvMapCapture::kDefaultIrradianceFaceSize,
            &irradianceError);
        if (!m_Irradiance)
        {
            ME_CORE_WARN(
                "EnvironmentMap: irradiance bake failed ({}); aliasing environment.",
                irradianceError);
            m_Irradiance = m_Environment;
        }

        std::string prefilterError;
        m_Prefilter = EnvMapCapture::PrefilterEnvironment(
            rhi,
            *m_Environment->GetRHITexture(),
            engineDefaultAssetsRoot,
            EnvMapCapture::kDefaultCubeFaceSize,
            &prefilterError);
        if (!m_Prefilter)
        {
            ME_CORE_WARN(
                "EnvironmentMap: prefilter bake failed ({}); aliasing environment.",
                prefilterError);
            m_Prefilter = m_Environment;
        }

        ME_CORE_INFO(
            "EnvironmentMap: baked sky/IBL from project HDR '{}'.",
            m_SourceHdrPath);
        return true;
    }

    bool EnvironmentMap::EnsureGPUResources(RHI& rhi)
    {
        if (m_GPUResourcesReady && m_Environment)
        {
            return true;
        }

        ReleaseGPUResources();

        const std::filesystem::path faceDirectory = ResolveProjectRelativeAbsolute(m_FaceDirectory);
        std::string error;

        if (!faceDirectory.empty() && !m_EnvironmentPrefix.empty())
        {
            m_Environment = TextureCubeLoader::LoadCubeMapFromDirectory(
                rhi,
                faceDirectory.string(),
                m_EnvironmentPrefix,
                true,
                &error);
            if (!m_Environment)
            {
                ME_CORE_WARN(
                    "EnvironmentMap '{}': environment faces missing under {} ({})",
                    m_FaceDirectory,
                    faceDirectory.string(),
                    error);
            }
        }

        if (!m_Environment && !m_SourceHdrPath.empty())
        {
            TryBakeFromSourceHdr(rhi);
        }

        if (!m_Environment)
        {
            m_Environment = CreateValidationEnvironmentCube(rhi);
            ME_CORE_WARN(
                "EnvironmentMap: using validation cube (add face PNGs or m_SourceHdrPath under Project Content).");
        }

        if (!m_Irradiance)
        {
            if (!faceDirectory.empty() && !m_IrradiancePrefix.empty())
            {
                error.clear();
                m_Irradiance = TextureCubeLoader::LoadCubeMapFromDirectory(
                    rhi,
                    faceDirectory.string(),
                    m_IrradiancePrefix,
                    false,
                    &error);
            }
            if (!m_Irradiance)
            {
                m_Irradiance = m_Environment;
            }
        }

        if (!m_Prefilter)
        {
            if (!faceDirectory.empty() && !m_PrefilterPrefix.empty())
            {
                error.clear();
                m_Prefilter = TextureCubeLoader::LoadCubeMapFromDirectory(
                    rhi,
                    faceDirectory.string(),
                    m_PrefilterPrefix,
                    true,
                    &error);
            }
            if (!m_Prefilter)
            {
                m_Prefilter = m_Environment;
            }
        }

        if (m_BrdfLUT && m_BrdfLUT->GetRHITexture() == nullptr)
        {
            if (m_BrdfLUT->GetMeta() != nullptr)
            {
                std::string loadError;
                std::shared_ptr<Texture2D> loaded =
                    AssetManager::Get().LoadAsset<Texture2D>(m_BrdfLUT->GetMeta()->AssetPath);
                if (loaded)
                {
                    m_BrdfLUT = loaded;
                }
                else
                {
                    ME_CORE_WARN("EnvironmentMap: BRDF LUT Texture2D has no GPU resource ({})", loadError);
                }
            }
        }

        m_GPUResourcesReady = m_Environment != nullptr;
        return m_GPUResourcesReady;
    }

    void EnvironmentMap::ReleaseGPUResources()
    {
        m_Environment.reset();
        m_Irradiance.reset();
        m_Prefilter.reset();
        m_GPUResourcesReady = false;
    }
}
