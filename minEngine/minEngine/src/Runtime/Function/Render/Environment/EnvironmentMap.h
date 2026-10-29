#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIDeferredCreate.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Function/Render/Texture.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace minEngine
{
    class RHI;
    class TextureCube;

    /**
     * Project-owned environment package (sky + IBL derivatives).
     * Must live under Project Content â€?AssetManager does not register EngineDefault paths.
     * Face textures are loaded from a project-relative directory + prefixes (TextureCube is not an Asset yet).
     */
    /**
     * Whether a bake may run on the calling thread right now.
     *
     * A bake reads assets, creates textures, draws six faces per mip level and submits its own
     * immediate command buffer. It therefore needs the graphics role and an uninterrupted command
     * stream, and both are properties of the calling moment rather than of the object: the same
     * call is correct at a frame start and a defect inside a recording window that has already
     * begun to build this frame's stream.
     */
    enum class EEnvironmentBakeAdmission : uint8_t
    {
        Allowed,
        /** The caller does not own the graphics context (an asset thread, the game role in Multi). */
        NotGraphicsRole,
        /** The caller already records a stream on this thread; a submit here would land inside it. */
        RecordingWindowOpen,
    };

    inline EEnvironmentBakeAdmission GetEnvironmentBakeAdmission()
    {
        if (!RHIGraphicsRole::IsCurrent())
        {
            return EEnvironmentBakeAdmission::NotGraphicsRole;
        }
        if (RHICommandList::IsRecordingWindowOpenOnThisThread())
        {
            return EEnvironmentBakeAdmission::RecordingWindowOpen;
        }
        return EEnvironmentBakeAdmission::Allowed;
    }

    /** What one bake attempt settled as, in the only distinction its retry policy can act on. */
    enum class EEnvironmentBakeOutcome : uint8_t
    {
        /** The map's GPU resources exist and may be sampled. */
        Ready,
        /** A GPU object the bake needs is still queued for the graphics role: the next try gets it. */
        RetryLater,
        /** Retrying produces the same failure; it was reported once and the map stays without one. */
        Failed,
    };

    /**
     * The retry policy for the bake.
     *
     * One bake is a whole GPU production: an asset load, six faces per mip level, mip generation
     * and an immediate submit. A failure that is not going to change within the frame - a missing
     * HDR, a driver that refused the program, a texture the device would not allocate - is
     * therefore retried on a growing quiet period instead of on the next frame, which is what turn
     * one reportable failure into a hitch on every frame.
     *
     * The two kinds of failure are kept apart because they answer differently. A queued handle is
     * an ordinary state right after a create off the graphics role: it backs off and keeps asking,
     * and it never gives up, because giving up would strand a resource that is about to exist. A
     * refusal is final for this object: it settles, reports once, and expects the asset to be
     * rebuilt (a reloaded asset is a fresh object with a fresh schedule).
     */
    class EnvironmentBakeSchedule
    {
    public:
        /** Quiet period after the first failed attempt, doubled by each failure after it. */
        static constexpr uint32_t kFirstQuietFrames = 30;
        /** Ceiling of the quiet period: a resource that arrives later still lands within it. */
        static constexpr uint32_t kMaxQuietFrames = 480;

        enum class EDecision : uint8_t
        {
            /** One attempt may run now. */
            Attempt,
            /** A previous attempt failed recently; this frame goes without the bake. */
            Defer,
            /** The failure was final; nothing runs until the asset is rebuilt. */
            Settled,
        };

        /** Once per frame, before anything else: may this frame run an attempt? */
        EDecision BeginFrame()
        {
            if (m_bSettled)
            {
                ++m_FramesWithoutAttempt;
                return EDecision::Settled;
            }
            if (m_QuietFramesRemaining > 0)
            {
                --m_QuietFramesRemaining;
                ++m_FramesWithoutAttempt;
                return EDecision::Defer;
            }
            return EDecision::Attempt;
        }

        /** The attempt produced the resources. */
        void NoteAttemptSucceeded()
        {
            m_QuietFramesRemaining = 0;
            m_QuietFrames = 0;
            m_FramesWithoutAttempt = 0;
        }

        /** The attempt granted by BeginFrame ran and produced nothing. */
        void NoteAttemptFailed(bool bRetryable)
        {
            ++m_Attempts;
            m_FramesWithoutAttempt = 0;
            if (!bRetryable)
            {
                m_bSettled = true;
                m_QuietFramesRemaining = 0;
                return;
            }

            m_QuietFrames = m_QuietFrames == 0 ? kFirstQuietFrames : m_QuietFrames * 2;
            if (m_QuietFrames > kMaxQuietFrames)
            {
                m_QuietFrames = kMaxQuietFrames;
            }
            m_QuietFramesRemaining = m_QuietFrames;
        }

        bool IsSettled() const { return m_bSettled; }
        uint32_t GetAttempts() const { return m_Attempts; }
        uint32_t GetQuietFrames() const { return m_QuietFrames; }
        uint32_t GetFramesWithoutAttempt() const { return m_FramesWithoutAttempt; }

    private:
        uint32_t m_Attempts = 0;
        uint32_t m_QuietFrames = 0;
        uint32_t m_QuietFramesRemaining = 0;
        uint32_t m_FramesWithoutAttempt = 0;
        bool m_bSettled = false;
    };

    ME_CLASS()
    class EnvironmentMap : public Asset
    {
        ME_GENERATED_BODY()

    public:
        EnvironmentMap() = default;
        ~EnvironmentMap() override = default;

        /**
         * Produce this map's GPU resources: the face textures, or the bake behind them.
         *
         * Graphics role only, and never inside a recording window - both are checked, and a caller
         * that breaks either is refused and reported instead of served. The renderer calls this once
         * per frame, before the frame opens its recording window; the passes that draw the sky only
         * read IsReadyForSky(), because a bake cannot run inside a frame that is already recording.
         *
         * A failure does not retry on the next frame; see EnvironmentBakeSchedule for what it does.
         */
        bool EnsureGPUResources(RHI& rhi);
        /** Drop every GPU resource. Graphics role only. */
        void ReleaseGPUResources();

        /**
         * Published readiness, readable from any role.
         *
         * The resource members below belong to the graphics role, which replaces them while a bake
         * runs, so a consumer on any other role must not dereference them. These answers come from
         * the flags that role sets once an attempt is over.
         */
        bool HasEnvironment() const { return m_ReadyForSky.load(std::memory_order_acquire); }
        bool IsReadyForSky() const { return m_ReadyForSky.load(std::memory_order_acquire); }
        bool IsReadyForPBR() const { return m_ReadyForPBR.load(std::memory_order_acquire); }

        /** Graphics role only: the texture members these return are written by the bake. */

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
         * Used when face PNGs are missing â€?GPU bake via EnvMapCapture.
         */
        ME_PROPERTY()
        std::string m_SourceHdrPath;

    private:
        EEnvironmentBakeOutcome BuildGPUResources(RHI& rhi);
        EEnvironmentBakeOutcome TryBakeFromSourceHdr(RHI& rhi);
        /** Graphics role: publish what the resource members now hold, for readers on other roles. */
        void PublishReadiness();
        void ReportBakeRefusal(EEnvironmentBakeAdmission admission) const;
        void ReportBakeFailure(EEnvironmentBakeOutcome outcome);

        std::shared_ptr<TextureCube> m_Environment;
        std::shared_ptr<TextureCube> m_Irradiance;
        std::shared_ptr<TextureCube> m_Prefilter;
        /** Graphics role only: this object's own view of whether m_Environment exists. */
        bool m_GPUResourcesReady = false;
        /** Graphics role only: the retry policy behind EnsureGPUResources. */
        EnvironmentBakeSchedule m_BakeSchedule;
        /** Written by PublishReadiness, read from every role. */
        std::atomic<bool> m_ReadyForSky{false};
        std::atomic<bool> m_ReadyForPBR{false};
        /** Bakes refused for the caller's thread or recording window; written from any role. */
        mutable std::atomic<uint64_t> m_RefusedBakeCount{0};
    };
}

#include "Generated/Reflection/EnvironmentMap.gen.h"
