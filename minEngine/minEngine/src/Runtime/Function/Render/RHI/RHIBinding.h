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

    enum class RHIBindingType : uint8_t
    {
        UniformBuffer,
        TextureSRV,
    };

    struct RHIBindingLayoutEntry
    {
        uint32_t Slot = 0;
        RHIBindingType Type = RHIBindingType::TextureSRV;
        uint32_t ShaderBinding = 0;
        RHIGraphicsShaderStage Visibility = RHIGraphicsShaderStage::Pixel;
    };

    class RHIBindingLayout
    {
    public:
        virtual ~RHIBindingLayout() = default;

        virtual const std::vector<RHIBindingLayoutEntry>& GetEntries() const = 0;
    };

    using RHIBindingLayoutRef = std::shared_ptr<RHIBindingLayout>;

    struct RHIBindingResource
    {
        RHIBindingType Type = RHIBindingType::TextureSRV;
        RHIBuffer* Buffer = nullptr;
        RHIShaderResourceView* TextureSRV = nullptr;
    };

    class RHIBindingSet
    {
    public:
        virtual ~RHIBindingSet() = default;

        virtual const RHIBindingLayout* GetLayout() const = 0;
        virtual const std::vector<RHIBindingResource>& GetResources() const = 0;
    };

    using RHIBindingSetRef = std::shared_ptr<RHIBindingSet>;
}
