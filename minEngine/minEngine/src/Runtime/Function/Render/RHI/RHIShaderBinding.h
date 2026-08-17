#pragma once

#include "Core.h"
#include "Render/RHI/RHITexture.h"

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

    class RHIShaderBindingSetLayout
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
    };

    class RHIShaderBindingSet
    {
    public:
        virtual ~RHIShaderBindingSet() = default;

        virtual const RHIShaderBindingSetLayout* GetLayout() const = 0;
        virtual const std::vector<RHIShaderBinding>& GetBindings() const = 0;
    };

    using RHIShaderBindingSetRef = std::shared_ptr<RHIShaderBindingSet>;
}
