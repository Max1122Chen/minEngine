#include "EnvironmentMap.h"

#include "EnvMapCapture.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Render/TextureCubeLoader.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"

#include <array>
#include <filesystem>
#include "Runtime/Core/Object/ObjectCast.h"

namespace minEngine
{
    namespace
    {
        std::shared_ptr<TextureCube> CreateValidationEnvironmentCube(RHI& rhi)
        {
            // Distinct face colors so Vulkan sky sampling failures are obvious (not near-black).
            // Mild sky-blue faces — fallback only when HDR bake / face PNGs are unavailable.
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
                ME_LOG(LogRender, Error, "EnvironmentMap: validation cubemap failed: {}", error);
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

    EEnvironmentBakeOutcome EnvironmentMap::TryBakeFromSourceHdr(RHI& rhi)
    {
        const std::filesystem::path hdrAbsolute = ResolveProjectRelativeAbsolute(m_SourceHdrPath);
        if (hdrAbsolute.empty() || !std::filesystem::exists(hdrAbsolute))
        {
            ME_LOG(LogRender, Warn, 
                "EnvironmentMap: SourceHdrPath '{}' not found under Project Content.",
                m_SourceHdrPath);
            return EEnvironmentBakeOutcome::Failed;
        }

        const std::filesystem::path& engineDefaultAssetsRoot =
            PathRegistry::Get().GetEngineDefaultAssetsRoot();
        if (engineDefaultAssetsRoot.empty())
        {
            ME_LOG(LogRender, Error, "EnvironmentMap: EngineDefaultAssetsRoot empty; cannot load EnvMap shaders.");
            return EEnvironmentBakeOutcome::Failed;
        }

        ImagePixels hdrPixels;
        std::string loadError;
        if (!ImageLoader::LoadHdr(hdrAbsolute.string(), hdrPixels, false, &loadError))
        {
            ME_LOG(LogRender, Warn, 
                "EnvironmentMap: failed to load HDR '{}' ({}).",
                hdrAbsolute.string(),
                loadError);
            return EEnvironmentBakeOutcome::Failed;
        }

        std::shared_ptr<Texture2D> equirect =
            Texture2DLoader::CreateFromHdrPixels(rhi, hdrPixels, hdrAbsolute.filename().string());
        ImageLoader::Free(hdrPixels);
        if (!equirect || !equirect->GetRHITexture())
        {
            ME_LOG(LogRender, Warn, "EnvironmentMap: failed to upload HDR '{}' to GPU.", hdrAbsolute.string());
            return EEnvironmentBakeOutcome::Failed;
        }

        // The capture records into a command buffer of its own. That is why the whole bake runs at
        // the frame start and never inside a frame's recording window: the caller checked both the
        // role and the window before it got here.
        rhi.RHIBeginImmediateCommands();

        std::string captureError;
        EEnvMapCaptureStatus captureStatus = EEnvMapCaptureStatus::Failed;
        m_Environment = EnvMapCapture::EquirectToCubemap(
            rhi,
            *equirect->GetRHITexture(),
            engineDefaultAssetsRoot,
            EnvMapCapture::kDefaultCubeFaceSize,
            &captureError,
            &captureStatus);
        if (!m_Environment)
        {
            rhi.RHIEndImmediateCommands();
            ME_LOG(LogRender, Warn, 
                "EnvironmentMap: EquirectToCubemap failed for '{}' ({}).",
                hdrAbsolute.string(),
                captureError);
            return captureStatus == EEnvMapCaptureStatus::RetryLater
                ? EEnvironmentBakeOutcome::RetryLater
                : EEnvironmentBakeOutcome::Failed;
        }

        // Vulkan irradiance/prefilter convolution deferred; sky only needs environment cube.
        if (RHIBackendSelection::IsVulkan())
        {
            m_Irradiance = m_Environment;
            m_Prefilter = m_Environment;
            ME_LOG(LogRender, Info, 
                "EnvironmentMap: baked sky cubemap from HDR '{}' (VK: IBL convolution deferred).",
                m_SourceHdrPath);
            return EEnvironmentBakeOutcome::Ready;
        }

        std::string irradianceError;
        EEnvMapCaptureStatus irradianceStatus = EEnvMapCaptureStatus::Failed;
        m_Irradiance = EnvMapCapture::ConvolveIrradiance(
            rhi,
            *m_Environment->GetRHITexture(),
            engineDefaultAssetsRoot,
            EnvMapCapture::kDefaultIrradianceFaceSize,
            &irradianceError,
            &irradianceStatus);
        if (!m_Irradiance)
        {
            if (irradianceStatus == EEnvMapCaptureStatus::RetryLater)
            {
                rhi.RHIEndImmediateCommands();
                ME_LOG(LogRender, Warn, 
                    "EnvironmentMap: irradiance bake is still waiting for its GPU objects ({}); retrying later.",
                    irradianceError);
                return EEnvironmentBakeOutcome::RetryLater;
            }
            ME_LOG(LogRender, Warn, 
                "EnvironmentMap: irradiance bake failed ({}); aliasing environment.",
                irradianceError);
            m_Irradiance = m_Environment;
        }

        std::string prefilterError;
        EEnvMapCaptureStatus prefilterStatus = EEnvMapCaptureStatus::Failed;
        m_Prefilter = EnvMapCapture::PrefilterEnvironment(
            rhi,
            *m_Environment->GetRHITexture(),
            engineDefaultAssetsRoot,
            EnvMapCapture::kDefaultCubeFaceSize,
            &prefilterError,
            &prefilterStatus);
        if (!m_Prefilter)
        {
            if (prefilterStatus == EEnvMapCaptureStatus::RetryLater)
            {
                rhi.RHIEndImmediateCommands();
                ME_LOG(LogRender, Warn, 
                    "EnvironmentMap: prefilter bake is still waiting for its GPU objects ({}); retrying later.",
                    prefilterError);
                return EEnvironmentBakeOutcome::RetryLater;
            }
            ME_LOG(LogRender, Warn, 
                "EnvironmentMap: prefilter bake failed ({}); aliasing environment.",
                prefilterError);
            m_Prefilter = m_Environment;
        }

        rhi.RHIEndImmediateCommands();

        ME_LOG(LogRender, Info, 
            "EnvironmentMap: baked sky/IBL from project HDR '{}'.",
            m_SourceHdrPath);
        return EEnvironmentBakeOutcome::Ready;
    }

    EEnvironmentBakeOutcome EnvironmentMap::BuildGPUResources(RHI& rhi)
    {
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
                ME_LOG(LogRender, Warn, 
                    "EnvironmentMap '{}': environment faces missing under {} ({})",
                    m_FaceDirectory,
                    faceDirectory.string(),
                    error);
            }
        }

        if (!m_Environment && !m_SourceHdrPath.empty())
        {
            // GL: full sky+IBL bake. VK: EquirectToCubemap + IBL alias (convolution still deferred).
            if (TryBakeFromSourceHdr(rhi) == EEnvironmentBakeOutcome::RetryLater)
            {
                // No placeholder is installed for a bake that is about to succeed: a validation
                // cube here would stay on screen and hide the map the retry brings.
                return EEnvironmentBakeOutcome::RetryLater;
            }
        }

        if (!m_Environment)
        {
            m_Environment = CreateValidationEnvironmentCube(rhi);
            ME_LOG(LogRender, Warn, 
                "EnvironmentMap: using validation cube (add face PNGs or m_SourceHdrPath under Project Content).");
        }

        if (!m_Environment)
        {
            return EEnvironmentBakeOutcome::Failed;
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
            // Loading an asset rewrites the AssetManager's tables and creates an object, which is
            // the game role's work. Doing it here would do both from the render role while this
            // frame is being recorded, so the reference is reported and the map keeps its other
            // resources: the game role loads it and the next bake picks it up.
            ME_LOG(LogRender, Warn, 
                "EnvironmentMap: the BRDF LUT reference '{}' has no GPU resource; loading an asset is the "
                "game role's work, so the map keeps drawing without it.",
                m_BrdfLUT->GetMeta() != nullptr ? m_BrdfLUT->GetMeta()->AssetPath : std::string("<unnamed>"));
        }

        m_GPUResourcesReady = true;
        return EEnvironmentBakeOutcome::Ready;
    }

    bool EnvironmentMap::EnsureGPUResources(RHI& rhi)
    {
        const EEnvironmentBakeAdmission admission = GetEnvironmentBakeAdmission();
        if (admission != EEnvironmentBakeAdmission::Allowed)
        {
            ReportBakeRefusal(admission);
            return false;
        }

        if (m_GPUResourcesReady && m_Environment)
        {
            return true;
        }

        // One call per frame: the renderer asks before it records, and the policy counts frames.
        switch (m_BakeSchedule.BeginFrame())
        {
        case EnvironmentBakeSchedule::EDecision::Attempt:
            break;
        case EnvironmentBakeSchedule::EDecision::Defer:
            return false;
        case EnvironmentBakeSchedule::EDecision::Settled:
            return false;
        }

        const EEnvironmentBakeOutcome outcome = BuildGPUResources(rhi);
        if (outcome == EEnvironmentBakeOutcome::Ready)
        {
            m_BakeSchedule.NoteAttemptSucceeded();
        }
        else
        {
            m_BakeSchedule.NoteAttemptFailed(outcome == EEnvironmentBakeOutcome::RetryLater);
            ReportBakeFailure(outcome);
        }

        PublishReadiness();
        return outcome == EEnvironmentBakeOutcome::Ready;
    }

    void EnvironmentMap::PublishReadiness()
    {
        // Single writer: this runs on the graphics role at the end of an attempt. Readers on other
        // roles get the answer here and never dereference the members above.
        const bool bReadyForSky =
            m_Environment != nullptr && m_Environment->GetRHITexture() != nullptr;
        const bool bReadyForPBR =
            m_Irradiance != nullptr && m_Irradiance->GetRHITexture() != nullptr;
        m_ReadyForSky.store(bReadyForSky, std::memory_order_release);
        m_ReadyForPBR.store(bReadyForPBR, std::memory_order_release);
    }

    void EnvironmentMap::ReportBakeRefusal(EEnvironmentBakeAdmission admission) const
    {
        const uint64_t refusals = m_RefusedBakeCount.fetch_add(1, std::memory_order_relaxed) + 1;
        if (refusals != 1 && refusals % 100 != 0)
        {
            return;
        }

        const char* what = admission == EEnvironmentBakeAdmission::NotGraphicsRole
            ? "the calling thread does not own the graphics context"
            : "this thread is already recording a command stream";
        ME_LOG(
            LogRender,
            Error,
            "EnvironmentMap '{}': the GPU bake was refused because {}; it creates textures and submits "
            "command buffers, so it belongs to the graphics role at the frame start, before the frame "
            "records. Nothing was created ({} refusal(s) so far).",
            !m_SourceHdrPath.empty() ? m_SourceHdrPath : m_FaceDirectory,
            what,
            refusals);
    }

    void EnvironmentMap::ReportBakeFailure(EEnvironmentBakeOutcome outcome)
    {
        if (outcome == EEnvironmentBakeOutcome::RetryLater)
        {
            ME_LOG(
                LogRender,
                Warn,
                "EnvironmentMap '{}': the GPU bake is waiting for objects the graphics role has not "
                "created yet (attempt {}); the next attempt is in {} frame(s) instead of on the next "
                "frame.",
                !m_SourceHdrPath.empty() ? m_SourceHdrPath : m_FaceDirectory,
                m_BakeSchedule.GetAttempts(),
                m_BakeSchedule.GetQuietFrames());
            return;
        }

        ME_LOG(
            LogRender,
            Error,
            "EnvironmentMap '{}': the GPU bake failed after {} attempt(s) and will not be retried on "
            "its own: one attempt is a full asset load, six faces per mip level and an immediate "
            "submit, so repeating it every frame only turns one failure into a hitch. Reload the "
            "asset to try again.",
            !m_SourceHdrPath.empty() ? m_SourceHdrPath : m_FaceDirectory,
            m_BakeSchedule.GetAttempts());
    }

    void EnvironmentMap::ReleaseGPUResources()
    {
        m_Environment.reset();
        m_Irradiance.reset();
        m_Prefilter.reset();
        m_GPUResourcesReady = false;
        PublishReadiness();
    }
}
