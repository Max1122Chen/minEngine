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

        bool IsValid() const
        {
            bool valid = ResourceType != ShadowResourceType::Invalid && Resolution.IsValid() && TextureUnit >= 0 && ResourcePtr != nullptr;
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

        std::shared_ptr<RHITexture2D> GetAs2D() const
        {
            if (ResourceType != ShadowResourceType::Depth2D)
            {
                return nullptr;
            }
            return std::static_pointer_cast<RHITexture2D>(ResourcePtr);
        }

        std::shared_ptr<RHITexture2DArray> GetAs2DArray() const
        {
            if (ResourceType != ShadowResourceType::Depth2DArray)
            {
                return nullptr;
            }
            return std::static_pointer_cast<RHITexture2DArray>(ResourcePtr);
        }

        std::shared_ptr<RHITextureCube> GetAsCube() const
        {
            if (ResourceType != ShadowResourceType::DepthCube)
            {
                return nullptr;
            }
            return std::static_pointer_cast<RHITextureCube>(ResourcePtr);
        }

    private:
        friend class ShadowResourceManager;
        std::shared_ptr<void> ResourcePtr; // This is a type-erased pointer to the actual resource, it can be a shared_ptr<RHITexture2DArray> or shared_ptr<RHITextureCube> depending on the ResourceType. We use this to manage the lifetime of the resource.
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
