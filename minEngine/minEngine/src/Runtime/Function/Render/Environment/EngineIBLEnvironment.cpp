#include "EngineIBLEnvironment.h"

#include "BrdfLutGenerator.h"
#include "EnvMapCapture.h"
#include "../Texture.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"
#include "../TextureCubeLoader.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <array>
#include <filesystem>

namespace minEngine
{
    std::shared_ptr<TextureCube> EngineIBLEnvironment::CreateValidationCube(RHI& rhi)
    {
        const std::array<uint8_t, 4> faceColors[6] = {
            std::array<uint8_t, 4>{ 255, 64, 64, 255 },
            std::array<uint8_t, 4>{ 64, 255, 64, 255 },
            std::array<uint8_t, 4>{ 64, 128, 255, 255 },
            std::array<uint8_t, 4>{ 255, 220, 64, 255 },
            std::array<uint8_t, 4>{ 220, 64, 255, 255 },
            std::array<uint8_t, 4>{ 64, 255, 220, 255 },
        };

        std::string error;
        std::shared_ptr<TextureCube> cube =
            TextureCubeLoader::CreateSolidColorCube(rhi, 32, faceColors, &error);
        if (!cube)
        {
            ME_CORE_ERROR("EngineIBLEnvironment: validation cubemap failed: {}", error);
        }
        return cube;
    }

    std::shared_ptr<TextureCube> EngineIBLEnvironment::TryLoadCubemapPrefix(
        RHI& rhi,
        const std::string& iblDirectory,
        const char* namePrefix,
        bool generateMipmaps)
    {
        if (!std::filesystem::is_directory(iblDirectory))
        {
            return nullptr;
        }

        std::string error;
        std::shared_ptr<TextureCube> cube = TextureCubeLoader::LoadCubeMapFromDirectory(
            rhi,
            iblDirectory,
            namePrefix,
            generateMipmaps,
            &error);
        if (!cube)
        {
            ME_CORE_WARN(
                "EngineIBLEnvironment: could not load {} cubemap from {} ({}).",
                namePrefix,
                iblDirectory,
                error);
        }
        return cube;
    }

    std::shared_ptr<TextureCube> EngineIBLEnvironment::TryLoadIrradianceFromDisk(
        RHI& rhi,
        const std::string& iblDirectory)
    {
        return TryLoadCubemapPrefix(rhi, iblDirectory, "irradiance", false);
    }

    std::shared_ptr<TextureCube> EngineIBLEnvironment::TryLoadEnvironmentFromDisk(
        RHI& rhi,
        const std::string& iblDirectory)
    {
        return TryLoadCubemapPrefix(rhi, iblDirectory, "environment", true);
    }

    std::shared_ptr<TextureCube> EngineIBLEnvironment::TryLoadPrefilterFromDisk(
        RHI& rhi,
        const std::string& iblDirectory)
    {
        return TryLoadCubemapPrefix(rhi, iblDirectory, "prefilter", true);
    }

    std::shared_ptr<TextureCube> EngineIBLEnvironment::TryCaptureEnvironmentFromHdr(
        RHI& rhi,
        const std::string& iblDirectory,
        const std::string& engineDefaultAssetsRoot)
    {
        if (!std::filesystem::is_directory(iblDirectory))
        {
            return nullptr;
        }

        std::filesystem::path hdrPath;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(iblDirectory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            if (!ImageLoader::IsHdrPath(entry.path().string()))
            {
                continue;
            }

            if (entry.path().filename() == "environment.hdr")
            {
                hdrPath = entry.path();
                break;
            }

            if (hdrPath.empty())
            {
                hdrPath = entry.path();
            }
        }

        if (hdrPath.empty())
        {
            return nullptr;
        }

        ImagePixels hdrPixels;
        std::string loadError;
        if (!ImageLoader::LoadHdr(hdrPath.string(), hdrPixels, false, &loadError))
        {
            ME_CORE_WARN(
                "EngineIBLEnvironment: failed to load HDR '{}' ({}).",
                hdrPath.string(),
                loadError);
            return nullptr;
        }

        std::shared_ptr<Texture2D> equirect =
            Texture2DLoader::CreateFromHdrPixels(rhi, hdrPixels, hdrPath.filename().string());
        ImageLoader::Free(hdrPixels);
        if (!equirect || !equirect->GetRHITexture())
        {
            ME_CORE_WARN(
                "EngineIBLEnvironment: failed to upload HDR '{}' to GPU.",
                hdrPath.string());
            return nullptr;
        }

        std::string captureError;
        std::shared_ptr<TextureCube> cube = EnvMapCapture::EquirectToCubemap(
            rhi,
            *equirect->GetRHITexture(),
            std::filesystem::path(engineDefaultAssetsRoot),
            EnvMapCapture::kDefaultCubeFaceSize,
            &captureError);
        if (!cube)
        {
            ME_CORE_WARN(
                "EngineIBLEnvironment: HDR cubemap capture failed for '{}' ({}).",
                hdrPath.string(),
                captureError);
            return nullptr;
        }

        ME_CORE_INFO(
            "EngineIBLEnvironment: captured {}x{} environment cubemap from HDR '{}'.",
            EnvMapCapture::kDefaultCubeFaceSize,
            EnvMapCapture::kDefaultCubeFaceSize,
            hdrPath.string());
        return cube;
    }

    std::shared_ptr<TextureCube> EngineIBLEnvironment::TryConvolveIrradianceFromEnvironment(
        RHI& rhi,
        const TextureCube& environmentCube,
        const std::string& engineDefaultAssetsRoot)
    {
        if (!environmentCube.GetRHITexture())
        {
            return nullptr;
        }

        std::string convolveError;
        std::shared_ptr<TextureCube> irradiance = EnvMapCapture::ConvolveIrradiance(
            rhi,
            *environmentCube.GetRHITexture(),
            std::filesystem::path(engineDefaultAssetsRoot),
            EnvMapCapture::kDefaultIrradianceFaceSize,
            &convolveError);
        if (!irradiance)
        {
            ME_CORE_WARN(
                "EngineIBLEnvironment: irradiance convolution failed ({}).",
                convolveError);
        }
        return irradiance;
    }

    std::shared_ptr<TextureCube> EngineIBLEnvironment::TryPrefilterEnvironmentFromEnvironment(
        RHI& rhi,
        const TextureCube& environmentCube,
        const std::string& engineDefaultAssetsRoot)
    {
        if (!environmentCube.GetRHITexture())
        {
            return nullptr;
        }

        const uint32_t faceSize = environmentCube.GetSize();
        if (faceSize == 0)
        {
            return nullptr;
        }

        std::string prefilterError;
        std::shared_ptr<TextureCube> prefilter = EnvMapCapture::PrefilterEnvironment(
            rhi,
            *environmentCube.GetRHITexture(),
            std::filesystem::path(engineDefaultAssetsRoot),
            faceSize,
            &prefilterError);
        if (!prefilter)
        {
            ME_CORE_WARN(
                "EngineIBLEnvironment: environment prefilter failed ({}).",
                prefilterError);
        }
        return prefilter;
    }

    std::shared_ptr<Texture2D> EngineIBLEnvironment::TryLoadBrdfLutFromDisk(
        RHI& rhi,
        const std::string& iblDirectory)
    {
        if (!std::filesystem::is_directory(iblDirectory))
        {
            return nullptr;
        }

        const std::filesystem::path lutPath = std::filesystem::path(iblDirectory) / "brdf_lut.png";
        if (!std::filesystem::is_regular_file(lutPath))
        {
            return nullptr;
        }

        ImagePixels pixels;
        std::string error;
        if (!ImageLoader::LoadLdr(lutPath.string(), pixels, false, &error))
        {
            ME_CORE_WARN("EngineIBLEnvironment: failed to load BRDF LUT '{}' ({}).", lutPath.string(), error);
            return nullptr;
        }

        std::shared_ptr<Texture2D> texture =
            Texture2DLoader::CreateFromPixels(rhi, pixels, "brdf_lut", GUID{});
        ImageLoader::Free(pixels);
        if (texture)
        {
            ME_CORE_INFO("EngineIBLEnvironment: loaded BRDF LUT from {}.", lutPath.string());
        }
        return texture;
    }

    std::shared_ptr<Texture2D> EngineIBLEnvironment::CreateIntegratedBrdfLut(RHI& rhi)
    {
        std::string error;
        std::shared_ptr<Texture2D> lut =
            BrdfLutGenerator::CreateIntegratedBrdfLut(rhi, BrdfLutGenerator::kDefaultLutSize, &error);
        if (!lut)
        {
            ME_CORE_WARN("EngineIBLEnvironment: integrated BRDF LUT failed ({}).", error);
        }
        return lut;
    }

    void EngineIBLEnvironment::Initialize(RHI* rhi, const std::string& engineDefaultAssetsRoot)
    {
        Shutdown();
        if (!rhi)
        {
            return;
        }

        m_RHI = rhi;
        m_EnvIntensity = 1.0f;
        m_IrradianceFromConvolution = false;
        m_PrefilterFromGpuPass = false;

        const std::string iblDirectory =
            engineDefaultAssetsRoot + "/Textures/IBL";

        m_Irradiance = TryLoadIrradianceFromDisk(*rhi, iblDirectory);

        m_Environment = TryLoadEnvironmentFromDisk(*rhi, iblDirectory);
        if (!m_Environment)
        {
            m_Environment = TryCaptureEnvironmentFromHdr(*rhi, iblDirectory, engineDefaultAssetsRoot);
        }

        m_UsingFallbackCube = false;
        if (!m_Environment)
        {
            m_Environment = CreateValidationCube(*rhi);
            m_UsingFallbackCube = m_Environment != nullptr;
            if (m_UsingFallbackCube)
            {
                ME_CORE_INFO(
                    "EngineIBLEnvironment: using 6-color validation environment cubemap (add *.hdr or environment_*.png under {}).",
                    iblDirectory);
            }
        }

        if (!m_Irradiance && m_Environment)
        {
            m_Irradiance = TryConvolveIrradianceFromEnvironment(
                *rhi,
                *m_Environment,
                engineDefaultAssetsRoot);
            m_IrradianceFromConvolution = m_Irradiance != nullptr;
        }

        if (!m_Irradiance && m_Environment)
        {
            m_Irradiance = m_Environment;
            ME_CORE_WARN(
                "EngineIBLEnvironment: irradiance aliased to environment cubemap (convolution unavailable).");
        }

        if (m_Irradiance && !m_UsingFallbackCube && !m_IrradianceFromConvolution)
        {
            ME_CORE_INFO("EngineIBLEnvironment: loaded irradiance cubemap from {}.", iblDirectory);
        }

        m_Prefilter = TryLoadPrefilterFromDisk(*rhi, iblDirectory);
        if (!m_Prefilter && m_Environment)
        {
            m_Prefilter = TryPrefilterEnvironmentFromEnvironment(
                *rhi,
                *m_Environment,
                engineDefaultAssetsRoot);
            m_PrefilterFromGpuPass = m_Prefilter != nullptr;
        }

        if (!m_Prefilter && m_Environment)
        {
            m_Prefilter = m_Environment;
            ME_CORE_INFO(
                "EngineIBLEnvironment: prefilter aliased to environment cubemap (GPU prefilter unavailable).");
        }
        else if (!m_Prefilter && m_Irradiance)
        {
            m_Prefilter = m_Irradiance;
            ME_CORE_WARN(
                "EngineIBLEnvironment: prefilter aliased to irradiance cubemap (no environment source).");
        }

        m_BrdfLUT = TryLoadBrdfLutFromDisk(*rhi, iblDirectory);
        if (!m_BrdfLUT)
        {
            m_BrdfLUT = CreateIntegratedBrdfLut(*rhi);
        }
    }

    void EngineIBLEnvironment::Shutdown()
    {
        m_BrdfLUT.reset();
        m_Prefilter.reset();
        m_Irradiance.reset();
        m_Environment.reset();
        m_RHI = nullptr;
        m_UsingFallbackCube = false;
        m_IrradianceFromConvolution = false;
        m_PrefilterFromGpuPass = false;
        m_EnvIntensity = 1.0f;
    }
}
