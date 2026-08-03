#pragma once

#include <cstdint>

namespace minEngine
{
    class RHITexture;

    static constexpr uint32_t RHIMaxColorRenderTargets = 8;

    enum class RHIRenderTargetLoadAction : uint8_t
    {
        NoAction,
        Load,
        Clear,
    };

    enum class RHIRenderTargetStoreAction : uint8_t
    {
        NoAction,
        Store,
    };

    // Packed load/store pair for a single color attachment (UE: ERenderTargetActions).
    enum class RHIRenderTargetActions : uint8_t
    {
        LoadOpMask = 2,

        DontLoadDontStore =
            (static_cast<uint8_t>(RHIRenderTargetLoadAction::NoAction) << LoadOpMask) |
            static_cast<uint8_t>(RHIRenderTargetStoreAction::NoAction),
        DontLoadStore =
            (static_cast<uint8_t>(RHIRenderTargetLoadAction::NoAction) << LoadOpMask) |
            static_cast<uint8_t>(RHIRenderTargetStoreAction::Store),
        ClearStore =
            (static_cast<uint8_t>(RHIRenderTargetLoadAction::Clear) << LoadOpMask) |
            static_cast<uint8_t>(RHIRenderTargetStoreAction::Store),
        LoadStore =
            (static_cast<uint8_t>(RHIRenderTargetLoadAction::Load) << LoadOpMask) |
            static_cast<uint8_t>(RHIRenderTargetStoreAction::Store),
        ClearDontStore =
            (static_cast<uint8_t>(RHIRenderTargetLoadAction::Clear) << LoadOpMask) |
            static_cast<uint8_t>(RHIRenderTargetStoreAction::NoAction),
        LoadDontStore =
            (static_cast<uint8_t>(RHIRenderTargetLoadAction::Load) << LoadOpMask) |
            static_cast<uint8_t>(RHIRenderTargetStoreAction::NoAction),
    };

    inline RHIRenderTargetActions MakeRenderTargetActions(
        RHIRenderTargetLoadAction load,
        RHIRenderTargetStoreAction store)
    {
        return static_cast<RHIRenderTargetActions>(
            (static_cast<uint8_t>(load) << static_cast<uint8_t>(RHIRenderTargetActions::LoadOpMask)) |
            static_cast<uint8_t>(store));
    }

    inline RHIRenderTargetLoadAction GetLoadAction(RHIRenderTargetActions action)
    {
        return static_cast<RHIRenderTargetLoadAction>(
            static_cast<uint8_t>(action) >> static_cast<uint8_t>(RHIRenderTargetActions::LoadOpMask));
    }

    inline RHIRenderTargetStoreAction GetStoreAction(RHIRenderTargetActions action)
    {
        return static_cast<RHIRenderTargetStoreAction>(
            static_cast<uint8_t>(action) &
            ((1u << static_cast<uint8_t>(RHIRenderTargetActions::LoadOpMask)) - 1u));
    }

    // Packed depth/stencil load-store (UE: EDepthStencilTargetActions).
    enum class RHIDepthStencilTargetActions : uint8_t
    {
        DepthMask = 4,

        DontLoadDontStore =
            (static_cast<uint8_t>(RHIRenderTargetActions::DontLoadDontStore) << DepthMask) |
            static_cast<uint8_t>(RHIRenderTargetActions::DontLoadDontStore),
        DontLoadStoreDepthStencil =
            (static_cast<uint8_t>(RHIRenderTargetActions::DontLoadStore) << DepthMask) |
            static_cast<uint8_t>(RHIRenderTargetActions::DontLoadStore),
        ClearDepthStencilStoreDepthStencil =
            (static_cast<uint8_t>(RHIRenderTargetActions::ClearStore) << DepthMask) |
            static_cast<uint8_t>(RHIRenderTargetActions::ClearStore),
        LoadDepthStencilStoreDepthStencil =
            (static_cast<uint8_t>(RHIRenderTargetActions::LoadStore) << DepthMask) |
            static_cast<uint8_t>(RHIRenderTargetActions::LoadStore),
        ClearDepthStencilDontStore =
            (static_cast<uint8_t>(RHIRenderTargetActions::ClearDontStore) << DepthMask) |
            static_cast<uint8_t>(RHIRenderTargetActions::ClearDontStore),
    };

    inline RHIDepthStencilTargetActions MakeDepthStencilTargetActions(
        RHIRenderTargetActions depth,
        RHIRenderTargetActions stencil)
    {
        return static_cast<RHIDepthStencilTargetActions>(
            (static_cast<uint8_t>(depth) << static_cast<uint8_t>(RHIDepthStencilTargetActions::DepthMask)) |
            static_cast<uint8_t>(stencil));
    }

    struct RHIClearValue
    {
        float Color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float Depth = 1.0f;
        uint32_t Stencil = 0;
    };

    // UE: FRHIRenderPassInfo — attachment list for RHICmdBeginRenderPass.
    class RHIRenderPassInfo
    {
    public:
        struct ColorAttachment
        {
            RHITexture* RenderTarget = nullptr;
            RHIRenderTargetActions Action = RHIRenderTargetActions::DontLoadDontStore;
            uint8_t MipIndex = 0;
            int32_t ArraySlice = -1;
        };

        struct DepthStencilAttachment
        {
            RHITexture* DepthStencilTarget = nullptr;
            RHIDepthStencilTargetActions Action = RHIDepthStencilTargetActions::DontLoadDontStore;
            uint8_t MipIndex = 0;
            int32_t ArraySlice = -1;
        };

        RHIRenderPassInfo() = default;

        explicit RHIRenderPassInfo(
            RHITexture* colorTarget,
            RHIRenderTargetActions colorAction,
            RHITexture* depthStencilTarget = nullptr,
            RHIDepthStencilTargetActions depthStencilAction = RHIDepthStencilTargetActions::DontLoadDontStore)
        {
            ColorAttachments[0].RenderTarget = colorTarget;
            ColorAttachments[0].Action = colorAction;
            DepthStencil.DepthStencilTarget = depthStencilTarget;
            DepthStencil.Action = depthStencilAction;
        }

        ColorAttachment ColorAttachments[RHIMaxColorRenderTargets] = {};
        DepthStencilAttachment DepthStencil = {};
        RHIClearValue ClearValue = {};
    };
}
