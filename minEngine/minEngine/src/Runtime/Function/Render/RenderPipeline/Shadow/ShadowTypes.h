#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"


namespace minEngine
{
    constexpr int MAX_SPOT_SHADOW_MAPS = 2;
    constexpr int MAX_POINT_SHADOW_MAPS = 2;
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

    struct ShadowResourceHandle
    {
        ShadowResourceType ResourceType = ShadowResourceType::Invalid;
        ShadowResolution Resolution{};

        int TextureUnit = -1;

        int ArrayBaseLayer = -1;
        int LayerCount = 0;

        RHITextureRef Texture;

        bool IsValid() const
        {
            const bool valid =
                ResourceType != ShadowResourceType::Invalid && Resolution.IsValid() && TextureUnit >= 0 && Texture != nullptr;
            switch (ResourceType)
            {
            case ShadowResourceType::Depth2D:
            case ShadowResourceType::DepthCube:
                return valid;
            case ShadowResourceType::Depth2DArray:
                return valid && ArrayBaseLayer >= 0 && LayerCount > 0;
            default:
                return false;
            }
        }

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
