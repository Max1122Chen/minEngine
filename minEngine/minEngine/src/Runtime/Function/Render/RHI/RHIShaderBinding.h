#pragma once

#include "Core.h"
#include "Render/RHI/RHIDeferredCreate.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHISampler.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace minEngine
{
    class RHIBuffer;
    class RHIShader;

    enum class RHIGraphicsShaderStage : uint8_t
    {
        Vertex,
        Pixel,
        /** Uniforms / SRVs visible to both graphics stages (Vulkan stageFlags). */
        All,
    };

    struct RHITextureSRVDesc
    {
        RHITexture* Texture = nullptr;
        uint8_t MipIndex = 0;
        int32_t ArraySlice = -1;
    };

    class RHIShaderResourceView
    {
    public:
        virtual ~RHIShaderResourceView() = default;

        virtual const RHITextureSRVDesc& GetCreateDesc() const = 0;
    };

    using RHIShaderResourceViewRef = std::shared_ptr<RHIShaderResourceView>;

    enum class RHIShaderBindingType : uint8_t
    {
        UniformBuffer,
        TextureSRV,
    };

    struct RHIShaderBindingSetLayoutEntry
    {
        uint32_t Slot = 0;
        RHIShaderBindingType Type = RHIShaderBindingType::TextureSRV;
        uint32_t ShaderBinding = 0;
        RHIGraphicsShaderStage Visibility = RHIGraphicsShaderStage::Pixel;
    };

    /**
     * The table a binding set is built from. Owns a native descriptor-set layout on APIs that have
     * one, and is a plain table on the ones that bind resources individually; the hand-off is the
     * same either way.
     */
    class RHIShaderBindingSetLayout : public RHIDeferredCreateTarget
    {
    public:
        virtual ~RHIShaderBindingSetLayout() = default;

        virtual const std::vector<RHIShaderBindingSetLayoutEntry>& GetEntries() const = 0;
    };

    using RHIShaderBindingSetLayoutRef = std::shared_ptr<RHIShaderBindingSetLayout>;

    struct RHIShaderBinding
    {
        RHIShaderBindingType Type = RHIShaderBindingType::TextureSRV;
        RHIBuffer* Buffer = nullptr;
        RHIShaderResourceView* TextureSRV = nullptr;
        /** Byte offset into Buffer for UniformBuffer (must respect min UBO alignment). */
        uint32_t BufferOffset = 0;
        /** Byte range; 0 means use the full buffer size from Buffer->GetDesc(). */
        uint32_t BufferRange = 0;
        /** Null uses RHI default LinearRepeat sampler. */
        RHISampler* Sampler = nullptr;
    };

    /** The resources bound together. Owns a driver object only on APIs that need one. */
    class RHIShaderBindingSet : public RHIDeferredCreateTarget
    {
    public:
        virtual ~RHIShaderBindingSet() = default;

        virtual const RHIShaderBindingSetLayout* GetLayout() const = 0;
        virtual const std::vector<RHIShaderBinding>& GetBindings() const = 0;
    };

    using RHIShaderBindingSetRef = std::shared_ptr<RHIShaderBindingSet>;
}
