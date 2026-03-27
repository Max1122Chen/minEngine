#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/LightComponent.h"

namespace minEngine
{
    enum class ShadowResourceType : uint8_t
    {
        Invalid = 0,
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

    struct ShadowLightKey
    {
        LightType Type = LightType::Directional;
        const void* LightProxyPtr = nullptr;

        bool operator==(const ShadowLightKey& rhs) const
        {
            return Type == rhs.Type && LightProxyPtr == rhs.LightProxyPtr;
        }
    };

    struct ShadowResourceHandle
    {
        ShadowResourceType ResourceType = ShadowResourceType::Invalid;
        int TextureUnit = -1;

        int ArrayBaseLayer = -1;
        int LayerCount = 0;

        int CubeIndex = -1;

        ShadowResolution Resolution{};
        bool Valid = false;
    };

    struct ShadowRequest
    {
        ShadowLightKey Key{};
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
        Matrix4 ViewProjFaces[6]{
            Matrix4(1.0f), Matrix4(1.0f), Matrix4(1.0f),
            Matrix4(1.0f), Matrix4(1.0f), Matrix4(1.0f)
        };

        int TargetLayer = -1;
        int TargetFace = -1;

        ShadowAtlasRect AtlasRect{};
    };
}
