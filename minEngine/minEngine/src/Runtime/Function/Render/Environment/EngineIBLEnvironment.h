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

    private:
        std::shared_ptr<TextureCube> CreateValidationCube(RHI& rhi);
        std::shared_ptr<TextureCube> TryLoadIrradianceFromDisk(RHI& rhi, const std::string& iblDirectory);
        std::shared_ptr<TextureCube> TryCaptureEnvironmentFromHdr(
            RHI& rhi,
            const std::string& iblDirectory,
            const std::string& engineDefaultAssetsRoot);

        RHI* m_RHI = nullptr;
        std::shared_ptr<TextureCube> m_Irradiance;
        std::shared_ptr<TextureCube> m_Prefilter;
        std::shared_ptr<Texture2D> m_BrdfLUT;
        bool m_UsingFallbackCube = false;
    };
}
