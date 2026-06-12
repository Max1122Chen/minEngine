#pragma once

#include "Core.h"
#include "Render/RHI/RHIShaderBinding.h"

#include <climits>
#include <cstdint>
#include <string>

namespace minEngine
{
    class RHITexture;
    /** Well-known logical texture names (string ids, RND-F01 P1). */
    inline constexpr const char* kRDGSceneColor = "SceneColor";
    inline constexpr const char* kRDGSceneDepth = "SceneDepth";
    inline constexpr const char* kRDGBackbuffer = "Backbuffer";
    inline constexpr const char* kRDGPostBufferA = "PostBufferA";

    enum class RDGTextureUsage : uint8_t
    {
        Unknown = 0,
        RenderTarget,
        ShaderResource,
        DepthWrite,
        DepthRead,
    };

    struct RDGTextureDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    /** Opaque handle into RenderGraph texture registry / frame slots. */
    class RDGTextureRef
    {
    public:
        RDGTextureRef() = default;

        bool IsValid() const { return m_Index != kInvalidIndex; }

        uint32_t GetIndex() const { return m_Index; }

        static RDGTextureRef FromIndex(uint32_t index) { return RDGTextureRef(index); }

    private:
        static constexpr uint32_t kInvalidIndex = UINT32_MAX;

        explicit RDGTextureRef(uint32_t index)
            : m_Index(index)
        {
        }

        uint32_t m_Index = kInvalidIndex;
    };

    struct RDGTextureSlot
    {
        std::string Name;
        RHITexture* Texture = nullptr;
        RHIShaderResourceViewRef SRV;
        RDGTextureUsage LastKnownUsage = RDGTextureUsage::Unknown;
    };

} // namespace minEngine
