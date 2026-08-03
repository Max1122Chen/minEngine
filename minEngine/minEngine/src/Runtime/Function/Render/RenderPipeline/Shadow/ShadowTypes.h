#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <string>

namespace minEngine
{
    constexpr int MAX_SPOT_SHADOW_MAPS = 2;
    constexpr int MAX_POINT_SHADOW_MAPS = 2;

    /** Legacy GL unit bases — layout description only (EngineShaderBindings). Not shadow slot indices. */
    constexpr int SPOT_SHADOW_MAP_BASE_UNIT = 9;
    constexpr int POINT_SHADOW_MAP_BASE_UNIT = 11;

    enum class ShadowResourceType : uint8_t
    {
        Invalid = 0,
        Depth2D,
        Depth2DArray,
        DepthCube
    };

    struct ShadowResolution
    {
        uint32_t Width = 0;
        uint32_t Height = 0;

        bool IsValid() const
        {
            return Width > 0 && Height > 0;
        }

        bool operator==(const ShadowResolution& rhs) const
        {
            return Width == rhs.Width && Height == rhs.Height;
        }

        bool operator!=(const ShadowResolution& rhs) const
        {
            return !(*this == rhs);
        }
    };

    /**
     * Sampling-slot descriptor (not a GPU resource owner).
     * Texture is filled by BindGraphShadowTextures after RDG SetupAttachments.
     * SlotIndex aligns with Set1 array index / LightUBO shadow index.
     */
    struct ShadowResourceHandle
    {
        ShadowResourceType ResourceType = ShadowResourceType::Invalid;
        ShadowResolution Resolution{};

        /** 0..MAX-1 for spot/point; 0 for directional atlas. */
        int SlotIndex = -1;

        int ArrayBaseLayer = -1;
        int LayerCount = 0;

        RHITextureRef Texture;

        bool IsValid() const
        {
            const bool meta =
                ResourceType != ShadowResourceType::Invalid && Resolution.IsValid() && SlotIndex >= 0;
            switch (ResourceType)
            {
            case ShadowResourceType::Depth2D:
            case ShadowResourceType::DepthCube:
                return meta;
            case ShadowResourceType::Depth2DArray:
                return meta && ArrayBaseLayer >= 0 && LayerCount > 0;
            default:
                return false;
            }
        }

        bool HasBoundTexture() const { return Texture != nullptr; }

        static ShadowResourceHandle InvalidHandle() { return ShadowResourceHandle{}; }
    };

    struct ShadowRequest
    {
        LightType Type = LightType::None;
        LightSceneProxy* LightProxy = nullptr;
        ShadowResolution Resolution{};
        uint32_t Priority = 0;
    };

    struct ShadowAtlasRect
    {
        // Reserved for future spot shadow atlas support.
        Vector2 TileOffset{0.0f, 0.0f};
        Vector2 TileScale{1.0f, 1.0f};
    };

    struct ShadowDrawCommand
    {
        LightType Type = LightType::Directional;
        ShadowResourceHandle Handle{};
        /** RDG logical depth name (shared across cascades/faces of the same map). */
        std::string GraphDepthResourceName;

        Matrix4 ViewProj = Matrix4(1.0f);
        Vector3 LightPosition{0.0f, 0.0f, 0.0f};
        float FarPlane = 0.0f;

        union
        {
            int TargetLayer = -1;
            int TargetFace;
        } Target;

        ShadowAtlasRect AtlasRect{};
    };
}
