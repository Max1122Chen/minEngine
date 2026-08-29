#include "RHIClipSpaceCapabilities.h"

#include "Render/RHI/RHIBackend.h"

namespace minEngine
{
    namespace
    {
        constexpr RHIClipSpaceCapabilities kOpenGLClipSpace{
            RHIClipDepthRange::NegativeOneToOne,
            RHITextureOriginY::Bottom,
            false,
        };

        constexpr RHIClipSpaceCapabilities kVulkanClipSpace{
            RHIClipDepthRange::ZeroToOne,
            RHITextureOriginY::Top,
            true,
        };

        constexpr RHIShadowPassCapabilities kOpenGLShadowPass{
            false,
            RHICullMode::Front,
            2.0f,
            4.0f,
        };

        // TD-025 scheme A: no shadow viewport flip; Front cull. Float D32 depth bias uses
        // Vulkan semantics (not glPolygonOffset units).
        constexpr RHIShadowPassCapabilities kVulkanShadowPass{
            false,
            RHICullMode::Front,
            1.5f,
            0.0f,
        };

        constexpr RHICubeCaptureCapabilities kOpenGLCubeCapture{false};
        constexpr RHICubeCaptureCapabilities kVulkanCubeCapture{false};
    }

    RHICullMode RHIShadowPassCapabilities::GetEffectiveCullMode() const
    {
        if (!ViewportFlipY)
        {
            return ReceiverFacingCullMode;
        }

        if (ReceiverFacingCullMode == RHICullMode::Front)
        {
            return RHICullMode::Back;
        }
        if (ReceiverFacingCullMode == RHICullMode::Back)
        {
            return RHICullMode::Front;
        }
        return RHICullMode::None;
    }

    const RHIClipSpaceCapabilities& GetClipSpaceCapabilities()
    {
        return RHIBackendSelection::IsVulkan() ? kVulkanClipSpace : kOpenGLClipSpace;
    }

    const RHIShadowPassCapabilities& GetShadowPassCapabilities()
    {
        return RHIBackendSelection::IsVulkan() ? kVulkanShadowPass : kOpenGLShadowPass;
    }

    const RHICubeCaptureCapabilities& GetCubeCaptureCapabilities()
    {
        return RHIBackendSelection::IsVulkan() ? kVulkanCubeCapture : kOpenGLCubeCapture;
    }

    bool GetViewportFlipY(RHIViewportConvention convention)
    {
        switch (convention)
        {
        case RHIViewportConvention::Scene:
            return GetClipSpaceCapabilities().SceneViewportFlipY;
        case RHIViewportConvention::ShadowMap2D:
            return GetShadowPassCapabilities().ViewportFlipY;
        case RHIViewportConvention::CubeMapFace:
            return GetCubeCaptureCapabilities().ViewportFlipY;
        default:
            return false;
        }
    }

    bool GetShadowMapSampleFlipY()
    {
        return GetClipSpaceCapabilities().TextureOriginY == RHITextureOriginY::Top;
    }

    float GetFrustumNdcZNear()
    {
        return GetClipSpaceCapabilities().ClipDepthRange == RHIClipDepthRange::ZeroToOne ? 0.0f : -1.0f;
    }

    float GetFrustumNdcZFar()
    {
        return 1.0f;
    }

    RHIImGuiSceneColorUv GetImGuiSceneColorUv()
    {
        if (GetClipSpaceCapabilities().TextureOriginY == RHITextureOriginY::Top)
        {
            return {0.0f, 0.0f, 1.0f, 1.0f};
        }
        return {0.0f, 1.0f, 1.0f, 0.0f};
    }

} // namespace minEngine
