#pragma once

#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHITexture.h"

#include <array>
#include <cstdint>

namespace minEngine
{
    class RHIShader;
    class VertexDefinition;

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
    };

    // UE: FRHIGraphicsPipelineState — opaque handle; backends may subclass.
    class RHIGraphicsPipelineState
    {
    };

    // UE: FGraphicsPipelineStateInitializer — immutable PSO description.
    class RHIGraphicsPSOCreateInfo
    {
    public:
        RHIShader* VertexShader = nullptr;
        RHIShader* PixelShader = nullptr;
        VertexDefinition* VertexDeclaration = nullptr;

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

    // UE: FRHIGraphicsPipelineStateFallBack — OpenGL-style state on the handle.
    class RHIGraphicsPSOStateFallback : public RHIGraphicsPipelineState
    {
    public:
        RHIGraphicsPSOStateFallback() = default;
        explicit RHIGraphicsPSOStateFallback(const RHIGraphicsPSOCreateInfo& createInfo)
            : m_CreateInfo(createInfo)
        {
        }

        const RHIGraphicsPSOCreateInfo& GetCreateInfo() const { return m_CreateInfo; }

    private:
        RHIGraphicsPSOCreateInfo m_CreateInfo;
    };
}
