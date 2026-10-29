#pragma once

#include "Render/RHI/RHIDeferredCreate.h"
#include "Render/RHI/RHIPipelineLayout.h"
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
        /** Factors are not yet desc-driven; OpenGL Apply uses SrcAlpha / OneMinusSrcAlpha when enabled. */
    };

    enum class RHIDepthCompareFunc : uint8_t
    {
        Less,
        LessEqual,
        Always,
    };

    enum class RHICullMode : uint8_t
    {
        None,
        Back,
        Front,
    };

    struct RHIDepthStencilStateDesc
    {
        bool bDepthTestEnabled = true;
        bool bDepthWriteEnabled = true;
        bool bStencilTestEnabled = false;
        RHIDepthCompareFunc DepthCompare = RHIDepthCompareFunc::Less;
    };

    struct RHIRasterizerStateDesc
    {
        bool bDepthClipEnabled = true;
        bool bCullEnabled = false;
        RHICullMode CullMode = RHICullMode::Back;
        // OpenGL: glPolygonOffset(factor, units). Used by depth-only shadow passes.
        float DepthBiasSlopeScale = 0.0f;
        float DepthBiasConstant = 0.0f;
    };

    /**
     * A graphics pipeline state.
     *
     * Whether the backend owns a native object for it depends on the API: OpenGL binds the state
     * piece by piece and the value is only a description, while other APIs compile a pipeline
     * object. Taking the same hand-off as the other resources means a backend that does own one
     * still creates it where the context is current, and a backend that does not is ready at once.
     */
    class RHIGraphicsPipelineState : public RHIDeferredCreateTarget
    {
    public:
        virtual ~RHIGraphicsPipelineState() = default;
    };

    using RHIGraphicsPipelineStateRef = std::shared_ptr<RHIGraphicsPipelineState>;

    class RHIGraphicsPSODesc
    {
    public:
        RHIPipelineLayout* PipelineLayout = nullptr;
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
