#pragma once

#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHITexture.h"

#include <array>
#include <cstdint>
#include <memory>

namespace minEngine
{
    class RHIShader;
    class RHIVertexInputLayout;

    enum class RHIPrimitiveType : uint8_t
    {
        TriangleList,
        TriangleStrip,
        LineList,
    };

    struct RHIBlendStateDesc
    {
        bool bBlendEnabled = false;
    };

    struct RHIDepthStencilStateDesc
    {
        bool bDepthTestEnabled = true;
        bool bDepthWriteEnabled = true;
        bool bStencilTestEnabled = false;
    };

    struct RHIRasterizerStateDesc
    {
        bool bDepthClipEnabled = true;
        bool bCullEnabled = false;
    };

    class RHIGraphicsPipelineState
    {
    public:
        virtual ~RHIGraphicsPipelineState() = default;
    };

    using RHIGraphicsPipelineStateRef = std::shared_ptr<RHIGraphicsPipelineState>;

    class RHIGraphicsPSODesc
    {
    public:
        RHIShader* VertexShader = nullptr;
        RHIShader* PixelShader = nullptr;
        RHIVertexInputLayout* VertexInputLayout = nullptr;

        RHIBlendStateDesc BlendState;
        RHIDepthStencilStateDesc DepthStencilState;
        RHIRasterizerStateDesc RasterizerState;
        RHIPrimitiveType PrimitiveType = RHIPrimitiveType::TriangleList;

        uint32_t RenderTargetsEnabled = 0;
        std::array<TextureFormat, RHIMaxColorRenderTargets> RenderTargetFormats = {};
        TextureFormat DepthStencilTargetFormat = TextureFormat::None;

        RHIRenderTargetLoadAction DepthTargetLoadAction = RHIRenderTargetLoadAction::NoAction;
        RHIRenderTargetStoreAction DepthTargetStoreAction = RHIRenderTargetStoreAction::NoAction;
        RHIRenderTargetLoadAction StencilTargetLoadAction = RHIRenderTargetLoadAction::NoAction;
        RHIRenderTargetStoreAction StencilTargetStoreAction = RHIRenderTargetStoreAction::NoAction;

        uint32_t NumSamples = 1;
    };

    class RHIGraphicsPSOStateFallback : public RHIGraphicsPipelineState
    {
    public:
        RHIGraphicsPSOStateFallback() = default;
        explicit RHIGraphicsPSOStateFallback(const RHIGraphicsPSODesc& createDesc)
            : m_Desc(createDesc)
        {
        }

        const RHIGraphicsPSODesc& GetDesc() const { return m_Desc; }

    private:
        RHIGraphicsPSODesc m_Desc;
    };
}
