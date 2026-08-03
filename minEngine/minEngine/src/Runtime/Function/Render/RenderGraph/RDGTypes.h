#pragma once

#include "Core.h"
#include "Render/RHI/RHITexture.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    enum class RDGSizeClass : uint8_t
    {
        Absolute = 0,
        SwapchainRelative,
        InputRelative,
    };

    enum class RDGAttachmentFlags : uint32_t
    {
        None = 0,
        Persistent = 1u << 0,
    };

    inline RDGAttachmentFlags operator|(RDGAttachmentFlags a, RDGAttachmentFlags b)
    {
        return static_cast<RDGAttachmentFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline RDGAttachmentFlags operator&(RDGAttachmentFlags a, RDGAttachmentFlags b)
    {
        return static_cast<RDGAttachmentFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    /** Logical attachment declaration (Granite AttachmentInfo). */
    struct RDGAttachmentInfo
    {
        RDGSizeClass SizeClass = RDGSizeClass::SwapchainRelative;
        float SizeX = 1.0f;
        float SizeY = 1.0f;
        std::string SizeRelativeName;
        TextureFormat Format = TextureFormat::None;
        uint32_t Samples = 1;
        uint32_t Levels = 1;
        uint32_t Layers = 1;
        RDGAttachmentFlags Flags = RDGAttachmentFlags::Persistent;
        RHITextureDimension Dimension = RHITextureDimension::Texture2D;
    };

    /** Physical size after Bake (Granite ResourceDimensions subset). */
    struct RDGResourceDimensions
    {
        TextureFormat Format = TextureFormat::None;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;
        uint32_t Layers = 1;
        uint32_t Levels = 1;
        uint32_t Samples = 1;
        RDGAttachmentFlags Flags = RDGAttachmentFlags::None;
        RHITextureCreateFlags Usage = RHITextureCreateFlags::None;
        RHITextureDimension Dimension = RHITextureDimension::Texture2D;
        std::string DebugName;
    };

    enum class RDGQueue : uint8_t
    {
        Graphics = 0,
    };

    /** Well-known logical texture names. */
    inline constexpr const char* kRDGSceneColor = "SceneColor";
    inline constexpr const char* kRDGSceneDepth = "SceneDepth";
    inline constexpr const char* kRDGBackbuffer = "Backbuffer";
    inline constexpr const char* kRDGPostBufferA = "PostBufferA";
    inline constexpr const char* kRDGDirShadowAtlas = "DirShadowAtlas";
}
