#pragma once

#include "Render/RHI/RHIGraphicsPipelineState.h"

#include <cstdint>

namespace minEngine
{
    enum class RHIClipDepthRange : uint8_t
    {
        NegativeOneToOne,
        ZeroToOne,
    };

    enum class RHITextureOriginY : uint8_t
    {
        Top,
        Bottom,
    };

    /** How RHICmdSetViewport maps clip Y to framebuffer rows for a pass category. */
    enum class RHIViewportConvention : uint8_t
    {
        Scene,
        ShadowMap2D,
        CubeMapFace,
    };

    struct RHIClipSpaceCapabilities
    {
        RHIClipDepthRange ClipDepthRange = RHIClipDepthRange::NegativeOneToOne;
        RHITextureOriginY TextureOriginY = RHITextureOriginY::Bottom;
        bool SceneViewportFlipY = false;
    };

    struct RHIShadowPassCapabilities
    {
        bool ViewportFlipY = false;
        RHICullMode ReceiverFacingCullMode = RHICullMode::Front;

        RHICullMode GetEffectiveCullMode() const;
    };

    struct RHICubeCaptureCapabilities
    {
        bool ViewportFlipY = false;
    };

    struct RHIImGuiSceneColorUv
    {
        float U0 = 0.0f;
        float V0 = 1.0f;
        float U1 = 1.0f;
        float V1 = 0.0f;
    };

    const RHIClipSpaceCapabilities& GetClipSpaceCapabilities();
    const RHIShadowPassCapabilities& GetShadowPassCapabilities();
    const RHICubeCaptureCapabilities& GetCubeCaptureCapabilities();

    bool GetViewportFlipY(RHIViewportConvention convention);
    float GetFrustumNdcZNear();
    float GetFrustumNdcZFar();
    RHIImGuiSceneColorUv GetImGuiSceneColorUv();

} // namespace minEngine
