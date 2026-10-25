#pragma once

#include "Core.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <memory>

namespace minEngine
{
    class RenderCamera;
    class RenderScene;
    class SceneViewport;
    class SceneRenderTarget;
    class RHI;
    class Texture2DRenderTarget;

    /**
     * One-stop offscreen renderer: owns a SceneViewport (RT + camera + view id),
     * assembles the SceneDrawDesc, and submits to the active renderer.
     *
     * Optional SetColorTarget binds a caller-owned Texture2DRenderTarget as SceneColor
     * (RDG external) so each bake keeps an independent durable texture.
     */
    class MINENGINE_API OffscreenRenderer
    {
    public:
        OffscreenRenderer();
        ~OffscreenRenderer();

        OffscreenRenderer(const OffscreenRenderer&) = delete;
        OffscreenRenderer& operator=(const OffscreenRenderer&) = delete;
        OffscreenRenderer(OffscreenRenderer&&) noexcept;
        OffscreenRenderer& operator=(OffscreenRenderer&&) noexcept;

        /** Create the offscreen RT + camera at the given size. Call once before rendering. */
        void Initialize(uint32_t width, uint32_t height);
        /** Release GPU resources. Safe to call multiple times. */
        void Shutdown();

        /** Scene to render (RenderScene from a PreviewScene or a game scene). */
        void SetScene(RenderScene* scene) { m_DrawDesc.Scene = scene; }
        RenderScene* GetScene() const { return m_DrawDesc.Scene; }

        void SetCamera(RenderCamera* camera) { m_DrawDesc.Camera = camera; }
        RenderCamera* GetCamera() { return m_DrawDesc.Camera; }

        /** Convenience: use the camera owned by the wrapped SceneViewport (1:1 world->screen). */
        RenderCamera* EnsureInternalCamera();

        /** Clear color applied to the offscreen color target (RGBA). Default transparent. */
        void SetClearColor(float r, float g, float b, float a);
        void SetClearColor(const float rgba[4]);
        const float* GetClearColor() const { return m_DrawDesc.ClearColor; }

        /** Depth clear applied to the offscreen depth target. */
        void SetDepthClear(float depth) { m_DrawDesc.DepthClear = depth; }
        float GetDepthClear() const { return m_DrawDesc.DepthClear; }

        /** Rendering flags. Use SceneDrawFlags::None for a bare mesh with transparent bg. */
        void SetFlags(SceneDrawFlags flags) { m_DrawDesc.Flags = flags; }
        SceneDrawFlags GetFlags() const { return m_DrawDesc.Flags; }

        const SceneDrawDesc& GetDrawDesc() const { return m_DrawDesc; }

        /**
         * Optional durable color target. When set, RenderOnce writes into this texture via
         * RDG RegisterExternal (SceneColor). Size must match Initialize width/height.
         */
        void SetColorTarget(const std::shared_ptr<Texture2DRenderTarget>& target);
        void ClearColorTarget();
        const std::shared_ptr<Texture2DRenderTarget>& GetColorTarget() const { return m_ColorTarget; }

        /**
         * Render one frame synchronously.
         * Returns the color RHI texture (target's GPU resource when SetColorTarget is used,
         * otherwise the graph-published SceneColor). Empty on failure.
         */
        const RHITextureRef& RenderOnce();

        /** Current offscreen color texture (after a successful render). */
        const RHITextureRef& GetColorTexture() const;
        const RHITextureRef& GetDepthTexture() const;

        uint32_t GetWidth() const { return m_Viewport ? m_Width : 0; }
        uint32_t GetHeight() const { return m_Viewport ? m_Height : 0; }

    private:
        RHI* ResolveRHI() const;
        void SyncViewportDesc();

        std::unique_ptr<SceneViewport> m_Viewport;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        SceneDrawDesc m_DrawDesc{};
        std::shared_ptr<Texture2DRenderTarget> m_ColorTarget;
        bool m_Initialized = false;
    };
}
