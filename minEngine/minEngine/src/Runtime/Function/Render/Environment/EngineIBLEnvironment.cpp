#include "EngineIBLEnvironment.h"

#include "../Texture.h"
#include "../TextureCubeLoader.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

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

    std::shared_ptr<TextureCube> EngineIBLEnvironment::TryLoadIrradianceFromDisk(
        RHI& rhi,
        const std::string& iblDirectory)
    {
        if (!std::filesystem::is_directory(iblDirectory))
        {
            return nullptr;
        }

        std::string error;
        std::shared_ptr<TextureCube> cube = TextureCubeLoader::LoadCubeMapFromDirectory(
            rhi,
            iblDirectory,
            "irradiance",
            false,
            &error);
        if (!cube)
        {
            ME_CORE_WARN(
                "EngineIBLEnvironment: could not load irradiance cubemap from {} ({}).",
                iblDirectory,
                error);
        }
        return cube;
    }

    void EngineIBLEnvironment::Initialize(RHI* rhi, const std::string& engineDefaultAssetsRoot)
    {
        Shutdown();
        if (!rhi)
        {
            return;
        }

        m_RHI = rhi;

        const std::string iblDirectory =
            engineDefaultAssetsRoot + "/Textures/IBL";
        m_Irradiance = TryLoadIrradianceFromDisk(*rhi, iblDirectory);
        m_UsingFallbackCube = false;
        if (!m_Irradiance)
        {
            m_Irradiance = CreateValidationCube(*rhi);
            m_UsingFallbackCube = m_Irradiance != nullptr;
            if (m_UsingFallbackCube)
            {
                ME_CORE_INFO(
                    "EngineIBLEnvironment: using 6-color validation cubemap (place irradiance_posx.png etc. under {}).",
                    iblDirectory);
            }
        }
        else
        {
            ME_CORE_INFO("EngineIBLEnvironment: loaded irradiance cubemap from {}.", iblDirectory);
        }

        // P4.2 will load prefilter + BRDF LUT; P4.1 keeps nullptr until assets exist.
        m_Prefilter = nullptr;
        m_BrdfLUT = nullptr;
    }

    void EngineIBLEnvironment::Shutdown()
    {
        m_Irradiance.reset();
        m_Prefilter.reset();
        m_BrdfLUT.reset();
        m_RHI = nullptr;
        m_UsingFallbackCube = false;
    }

    void EngineIBLEnvironment::BindForPBRDraw(RHIShader& shader) const
    {
        if (m_Irradiance && m_Irradiance->GetRHITexture())
        {
            m_Irradiance->GetRHITexture()->Bind(kEngineIBLIrradianceTextureUnit);
            shader.UploadUniformInt("u_EnvIrradianceMap", kEngineIBLIrradianceTextureUnit);
        }

        if (m_Prefilter && m_Prefilter->GetRHITexture())
        {
            m_Prefilter->GetRHITexture()->Bind(kEngineIBLPrefilterTextureUnit);
            shader.UploadUniformInt("u_EnvPrefilterMap", kEngineIBLPrefilterTextureUnit);
        }
        else if (m_Irradiance && m_Irradiance->GetRHITexture())
        {
            // P4.1: alias irradiance so PBR shaders can compile-sample prefilter slot in tests.
            m_Irradiance->GetRHITexture()->Bind(kEngineIBLPrefilterTextureUnit);
            shader.UploadUniformInt("u_EnvPrefilterMap", kEngineIBLPrefilterTextureUnit);
        }

        if (m_BrdfLUT && m_BrdfLUT->GetRHITexture())
        {
            m_BrdfLUT->GetRHITexture()->Bind(kEngineIBLBrdfLutTextureUnit);
            shader.UploadUniformInt("u_EnvBrdfLUT", kEngineIBLBrdfLutTextureUnit);
        }
    }
}
