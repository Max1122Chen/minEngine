#pragma once

#include "Core.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Function/Render/Texture.h"

#include <memory>
#include <string>

namespace minEngine
{
    class RHI;
    class TextureCube;

    /**
     * Project-owned environment package (sky + IBL derivatives).
     * Must live under Project Content — AssetManager does not register EngineDefault paths.
     * Face textures are loaded from a project-relative directory + prefixes (TextureCube is not an Asset yet).
     */
    ME_CLASS()
    class EnvironmentMap : public Asset
    {
        ME_GENERATED_BODY(EnvironmentMap)

    public:
        EnvironmentMap() = default;
        ~EnvironmentMap() override = default;

        bool EnsureGPUResources(RHI& rhi);
        void ReleaseGPUResources();

        bool HasEnvironment() const { return m_Environment != nullptr; }
        bool IsReadyForSky() const { return m_Environment != nullptr && m_Environment->GetRHITexture() != nullptr; }
        bool IsReadyForPBR() const
        {
            return m_Irradiance != nullptr && m_Irradiance->GetRHITexture() != nullptr;
        }

        TextureCube* GetEnvironment() const { return m_Environment.get(); }
        TextureCube* GetIrradiance() const { return m_Irradiance.get(); }
        TextureCube* GetPrefilter() const { return m_Prefilter.get(); }
        Texture2D* GetBrdfLUT() const { return m_BrdfLUT.get(); }

        /** Project-relative directory containing `{prefix}_{posx|...}.png` faces. */
        ME_PROPERTY()
        std::string m_FaceDirectory;

        ME_PROPERTY()
        std::string m_EnvironmentPrefix = "environment";

        ME_PROPERTY()
        std::string m_IrradiancePrefix = "irradiance";

        ME_PROPERTY()
        std::string m_PrefilterPrefix = "prefilter";

        /** Optional project Texture2D (e.g. copied brdf_lut.png). Shared across maps is fine. */
        ME_PROPERTY()
        std::shared_ptr<Texture2D> m_BrdfLUT;

        /**
         * Project-relative HDR equirect path (e.g. Textures/IBL/foo.hdr).
         * Used when face PNGs are missing — GPU bake via EnvMapCapture.
         */
        ME_PROPERTY()
        std::string m_SourceHdrPath;

    private:
        bool TryBakeFromSourceHdr(RHI& rhi);

        std::shared_ptr<TextureCube> m_Environment;
        std::shared_ptr<TextureCube> m_Irradiance;
        std::shared_ptr<TextureCube> m_Prefilter;
        bool m_GPUResourcesReady = false;
    };
}

#include "Generated/Reflection/EnvironmentMap.gen.h"
