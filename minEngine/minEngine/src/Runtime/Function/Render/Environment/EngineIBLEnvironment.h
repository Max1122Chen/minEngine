#pragma once

#include "Core.h"

#include <memory>
#include <string>

namespace minEngine
{
    class RHI;
    class RHIShader;
    class Texture2D;
    class TextureCube;

    /** Texture units for global IBL samplers (keep clear of material u_Texture0..N and shadow maps). */
    constexpr int kEngineIBLIrradianceTextureUnit = 4;
    constexpr int kEngineIBLPrefilterTextureUnit = 5;
    constexpr int kEngineIBLBrdfLutTextureUnit = 6;

    class EngineIBLEnvironment
    {
    public:
        void Initialize(RHI* rhi, const std::string& engineDefaultAssetsRoot);
        void Shutdown();

        bool HasIrradiance() const { return m_Irradiance != nullptr; }
        bool IsReadyForPBR() const { return m_Irradiance != nullptr; }

        void BindForPBRDraw(RHIShader& shader) const;

        const TextureCube* GetIrradiance() const { return m_Irradiance.get(); }
        const TextureCube* GetPrefilter() const { return m_Prefilter.get(); }
        const Texture2D* GetBrdfLUT() const { return m_BrdfLUT.get(); }

    private:
        std::shared_ptr<TextureCube> CreateValidationCube(RHI& rhi);
        std::shared_ptr<TextureCube> TryLoadCubemapPrefix(
            RHI& rhi,
            const std::string& iblDirectory,
            const char* namePrefix,
            bool generateMipmaps);
        std::shared_ptr<TextureCube> TryLoadIrradianceFromDisk(RHI& rhi, const std::string& iblDirectory);
        std::shared_ptr<TextureCube> TryLoadPrefilterFromDisk(RHI& rhi, const std::string& iblDirectory);
        std::shared_ptr<TextureCube> TryCaptureEnvironmentFromHdr(
            RHI& rhi,
            const std::string& iblDirectory,
            const std::string& engineDefaultAssetsRoot);
        std::shared_ptr<Texture2D> TryLoadBrdfLutFromDisk(RHI& rhi, const std::string& iblDirectory);
        std::shared_ptr<Texture2D> CreateIntegratedBrdfLut(RHI& rhi);

        RHI* m_RHI = nullptr;
        std::shared_ptr<TextureCube> m_Irradiance;
        std::shared_ptr<TextureCube> m_Prefilter;
        std::shared_ptr<Texture2D> m_BrdfLUT;
        float m_EnvIntensity = 1.0f;
        bool m_UsingFallbackCube = false;
    };
}
